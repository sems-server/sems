/*
 * Copyright (C) 2007 iptego GmbH
 * Copyright (C) 2010-2011 Stefan Sayer
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * For a license to use the sems software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * SEMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "XMLRPC2DI.h"

#include "AmPlugIn.h"
#include "log.h"
#include "AmConfigReader.h"
#include "AmUtils.h"
#include "AmArg.h"
#include "AmSessionContainer.h"
#include "AmEventDispatcher.h"
#include "TOXmlRpcClient.h"

#include <exception>

#define MOD_NAME "xmlrpc2di"

#define XMLRPC_PORT   "8090" // default port
EXPORT_PLUGIN_CLASS_FACTORY(XMLRPC2DI, MOD_NAME)

XMLRPC2DI* XMLRPC2DI::_instance=0;

// retry a failed server after 10 seconds
unsigned int XMLRPC2DI::ServerRetryAfter = 10; 

bool XMLRPC2DI::DebugServerParams = false;
bool XMLRPC2DI::DebugServerResult = false;

double XMLRPC2DI::ServerTimeout = -1;

XMLRPC2DI* XMLRPC2DI::instance()
{
  if(_instance == NULL){
    _instance = new XMLRPC2DI(MOD_NAME);
  }
  return _instance;
}

XMLRPC2DI::XMLRPC2DI(const string& mod_name) 
  : AmDynInvokeFactory(mod_name), configured(false)
{
}

int XMLRPC2DI::onLoad() {
  return instance()->load();
}

int XMLRPC2DI::load() {
  if (configured)    // load only once
    return 0;
  configured = true;
  
  AmConfigReader cfg;
  if(cfg.loadFile(AmConfig::ModConfigPath + string(MOD_NAME ".conf")))
    return -1;

  string multithreaded = cfg.getParameter("multithreaded", "yes");

  DebugServerResult = cfg.getParameter("debug_server_result", "no") == "yes";
  DebugServerParams = cfg.getParameter("debug_server_params", "no") == "yes";

  XmlRpcServer* s;
  unsigned int threads = 0;
  if (multithreaded == "yes") {
    if (!cfg.getParameter("threads").length())
      threads = 5;
    else 
      threads = cfg.getParameterInt("threads", 5);

    DBG("Running multi-threaded XMLRPC server with %u threads\n", threads);
    MultithreadXmlRpcServer* mt_s = new MultithreadXmlRpcServer();
    mt_s->createThreads(threads);    
    s = mt_s;
  } else {
    DBG("Running single-threaded XMLRPC server\n");
    s = new XmlRpcServer();
  }

  ServerRetryAfter = cfg.getParameterInt("server_retry_after", 10);
  DBG("retrying failed server after %u seconds\n", ServerRetryAfter);

  string server_timeout = cfg.getParameter("server_timeout");
  if (!server_timeout.empty()) {
    unsigned int server_timeout_i = 0;
    if (str2i(server_timeout, server_timeout_i)) {
      ERROR("could not understand server_timeout=%s\n", 
	    server_timeout.c_str());
      return -1;
    }
    
    if (server_timeout_i) {
      ServerTimeout = (double)server_timeout_i/1000.0; // in millisec
    }
  }

  string run_server = cfg.getParameter("run_server","yes");
  if (run_server != "yes") {
    DBG("XMLRPC server will not be started.\n");
    return 0;
  }

  string bind_ip = cfg.getParameter("server_ip");
  if (bind_ip.empty()) {
    DBG("binding on ANY interface\n");
  } else {
    bind_ip = fixIface2IP(bind_ip, false);
  }

  string conf_xmlrpc_port = cfg.getParameter("xmlrpc_port",XMLRPC_PORT);
  if (conf_xmlrpc_port.empty()) {
    ERROR("configuration: xmlrpc_port must be defined!\n");
    return -1;
  } 
  
  if (str2i(conf_xmlrpc_port, XMLRPCPort)) {
    ERROR("configuration: unable to decode xmlrpc_port value '%s'!\n", 
	  conf_xmlrpc_port.c_str());
    return -1;
  }

  bool export_di = false;
  string direct_export = cfg.getParameter("direct_export","");
  if (direct_export.length()) {
    DBG("direct_export interfaces: %s\n", direct_export.c_str());
  } else {
    DBG("No direct_export interfaces.\n");
  }

  string export_di_s = cfg.getParameter("export_di","yes");
  if (export_di_s == "yes") {
    export_di = true;
  } 
  
  INFO("XMLRPC Server: %snabling builtin method 'di'.\n", export_di?"E":"Not e");


  server = new XMLRPC2DIServer(XMLRPCPort, bind_ip, export_di, direct_export, s);
  if (!server->initialize()) {
    return -1;
  }

  server->start();
  server->waitUntilStarted();

  return 0;
}

XMLRPCServerEntry::XMLRPCServerEntry(string s, int p, string u)
  : active(true), last_try(0), server(s), port(p), uri(u)
{ }

XMLRPCServerEntry::~XMLRPCServerEntry() 
{ }

bool XMLRPCServerEntry::is_active() {
  if (!active && 
      ((unsigned int)(last_try + XMLRPC2DI::ServerRetryAfter) 
       < (unsigned int)time(NULL)))
      active = true;
  
  return active;
}

void XMLRPCServerEntry::set_failed() {
  active = false;
  time(&last_try);
}

void XMLRPC2DI::newConnection(const AmArg& args, AmArg& ret) {
  string app_name     = args.get(0).asCStr();
  string server_name  = args.get(1).asCStr();
  int port            = args.get(2).asInt();
  string uri          = args.get(3).asCStr();
  DBG("adding XMLRPC server http://%s:%d%s for application '%s'\n",
      server_name.c_str(), port, uri.c_str(), app_name.c_str());

  XMLRPCServerEntry* sc = new XMLRPCServerEntry(server_name, port, uri);

  server_mut.lock();
  servers.insert(std::make_pair(app_name, sc));
  server_mut.unlock();
}

XMLRPCServerEntry* XMLRPC2DI::getServer(const string& app_name) {
  vector<XMLRPCServerEntry*> scs;    
  server_mut.lock();
  for (multimap<string, XMLRPCServerEntry*>::iterator it=
	 servers.lower_bound(app_name);
       it != servers.upper_bound(app_name); it++) {
    if (it->second->is_active())
      scs.push_back(it->second);
  }
  server_mut.unlock();

  DBG("found %zd active connections for application %s\n", 
      scs.size(), app_name.c_str());  
  if (scs.empty()) {
    // no connections found
    return NULL;
  }

  // select one connection randomly 
  return scs[random() % scs.size()];
}

void XMLRPC2DI::sendRequest(const AmArg& args, AmArg& ret) {
  string app_name     = args.get(0).asCStr();
  string method       = args.get(1).asCStr();
  AmArg& params       = args.get(2);

  while (true) {
    XMLRPCServerEntry* srv = getServer(app_name);
    if (NULL == srv) {
      ret.push(-1);
      ret.push("no active connections");
      return;
    }
    TOXmlRpcClient c((const char*)srv->server.c_str(), (int)srv->port, 
		   (const char*)srv->uri.empty()?NULL:srv->uri.c_str()
#ifdef HAVE_XMLRPCPP_SSL
		   , false
#endif
		   );

    XmlRpcValue x_args, x_result;
    XMLRPC2DIServer::amarg2xmlrpcval(params, x_args);

    if (c.execute(method.c_str(), x_args, x_result, XMLRPC2DI::ServerTimeout) &&  !c.isFault()) {
      DBG("successfully executed method %s on server %s:%d\n",
	  method.c_str(), srv->server.c_str(), srv->port);
      ret.push(0);
      ret.push("OK");
      ret.assertArray(3);
      XMLRPC2DIServer::xmlrpcval2amarg(x_result, ret[2]);
      return;      
    } else {
      DBG("executing method %s failed on server %s:%d\n",
	  method.c_str(), srv->server.c_str(), srv->port);
      srv->set_failed();
    }
  }
}

void XMLRPC2DI::sendRequestList(const AmArg& args, AmArg& ret) {
  string app_name     = args.get(0).asCStr();
  string method       = args.get(1).asCStr();

  while (true) {
    XMLRPCServerEntry* srv = getServer(app_name);
    if (NULL == srv) {
      ret.push(-1);
      ret.push("no active connections");
      return;
    }
    TOXmlRpcClient c((const char*)srv->server.c_str(), (int)srv->port, 
		   (const char*)srv->uri.empty()?NULL:srv->uri.c_str()
#ifdef HAVE_XMLRPCPP_SSL
		   , false
#endif
		   );

    XmlRpcValue x_args, x_result;

    x_args.setSize(args.size()-2);
    for (size_t i=2;i<args.size();i++) {
      XMLRPC2DIServer::amarg2xmlrpcval(args.get(i), x_args[i-2]);
    }

    if (c.execute(method.c_str(), x_args, x_result, XMLRPC2DI::ServerTimeout) &&  !c.isFault()) {
      DBG("successfully executed method %s on server %s:%d\n",
	  method.c_str(), srv->server.c_str(), srv->port);
      ret.push(0);
      ret.push("OK");
      XMLRPC2DIServer::xmlrpcval2amarg(x_result, ret);
      return;      
    } else {
      DBG("executing method %s failed on server %s:%d\n",
	  method.c_str(), srv->server.c_str(), srv->port);
      srv->set_failed();
    }
  }
}

void XMLRPC2DI::invoke(const string& method, 
		       const AmArg& args, AmArg& ret) {

  if(method == "newConnection"){
    args.assertArrayFmt("ssis"); // app, server, port, uri
    newConnection(args, ret);
  } else if(method == "sendRequest"){
    args.assertArrayFmt("ssa");   // app, method, args
    sendRequest(args, ret);
  } else if(method == "sendRequestList"){
    args.assertArrayFmt("ss");   // app, method, ...
    sendRequestList(args, ret);
  } else if(method == "_list"){ 
    ret.push(AmArg("newConnection"));
    ret.push(AmArg("sendRequest"));
    ret.push(AmArg("sendRequestList"));
  }  else
    throw AmDynInvoke::NotImplemented(method);
  
}

// XMLRPC server functions

XMLRPC2DIServer::XMLRPC2DIServer(unsigned int port,
				 const string& bind_ip,
				 bool di_export, 
				 string direct_export,
				 XmlRpcServer* s) 
  : AmEventQueue(this),
    s(s),
    port(port),
    bind_ip(bind_ip), running(false),
    // register method 'calls'
    calls_method(s),
    // register method 'set_loglevel'
    setloglevel_method(s),
    // register method 'get_loglevel'
    getloglevel_method(s),
    // register method 'set_shutdownmode'
    setshutdownmode_method(s),
    // register method 'get_shutdownmode'
    getshutdownmode_method(s),
    getsessioncount_method(s),
    getcallsavg_method(s),
    getcallsmax_method(s),
    getcpsavg_method(s),
    getcpsmax_method(s),
    setcpslimit_method(s),
    getcpslimit_method(s)
{	
  INFO("XMLRPC Server: enabled builtin method 'calls'\n");
  INFO("XMLRPC Server: enabled builtin method 'get_loglevel'\n");
  INFO("XMLRPC Server: enabled builtin method 'set_loglevel'\n");
  INFO("XMLRPC Server: enabled builtin method 'get_shutdownmode'\n");
  INFO("XMLRPC Server: enabled builtin method 'set_shutdownmode'\n");
  INFO("XMLRPC Server: enabled builtin method 'get_sessioncount'\n");
  INFO("XMLRPC Server: enabled builtin method 'get_callsavg'\n");
  INFO("XMLRPC Server: enabled builtin method 'get_callsmax'\n");
  INFO("XMLRPC Server: enabled builtin method 'get_cpsavg'\n");
  INFO("XMLRPC Server: enabled builtin method 'get_cpsmax'\n");
  INFO("XMLRPC Server: enabled builtin method 'get_cpslimit'\n");
  INFO("XMLRPC Server: enabled builtin method 'set_cpslimit'\n");

  // export all methods via 'di' function? 
  if (di_export) {
    // register method 'di'
    di_method = new XMLRPC2DIServerDIMethod(s);
  }
  
  vector<string> export_ifaces = explode(direct_export, ";");
  for(vector<string>::iterator it=export_ifaces.begin(); 
      it != export_ifaces.end(); it++) {
    registerMethods(*it);
  }

  INFO("Initialized XMLRPC2DIServer with: \n");
  INFO("    IP = %s             port = %u\n", 
       bind_ip.empty()?"ANY":bind_ip.c_str(), port);
}

/** register all methods on xmlrpc server listed by the iface 
 *    in _list function 
 */
void XMLRPC2DIServer::registerMethods(const std::string& iface) {
  try {
    AmDynInvokeFactory* di_f = AmPlugIn::instance()->getFactory4Di(iface);
    if(NULL == di_f){
      ERROR("DI interface '%s' could not be found. Missing load_plugins?\n", 
	    iface.c_str());
      return;
    } 
    
    AmDynInvoke* di = di_f->getInstance();
    if(NULL == di){
      ERROR("could not get DI instance from '%s'.\n", 
	    iface.c_str());
      return;
    } 
    AmArg dummy, fct_list;
    di->invoke("_list", dummy, fct_list);

    for (unsigned int i=0;i<fct_list.size();i++) {
      string method = fct_list.get(i).asCStr();
      // see whether method already registered
      bool has_method = (NULL != s->findMethod(method));
      if (has_method) {
	ERROR("name conflict for method '%s' from interface '%s', "
	      "method already exported!\n",
	      method.c_str(), iface.c_str());
	ERROR("This method will be exported only as '%s.%s'\n",
	      iface.c_str(), method.c_str());
      }
      
      if (!has_method) {
	INFO("XMLRPC Server: enabling method '%s'\n",
	     method.c_str());
	DIMethodProxy* mp = new DIMethodProxy(method, method, di_f);
	s->addMethod(mp);
      }
      
      INFO("XMLRPC Server: enabling method '%s.%s'\n",
	   iface.c_str(), method.c_str());
      DIMethodProxy* mp = new DIMethodProxy(iface + "." + method, 
					    method, di_f);
      s->addMethod(mp);
    }
  } catch (AmDynInvoke::NotImplemented& e) {
    ERROR("Not implemented in interface '%s': '%s'\n", 
	  iface.c_str(), e.what.c_str());
  } catch (const AmArg::OutOfBoundsException& e) {
    ERROR("Out of bounds exception occured while exporting interface '%s'\n", 
	  iface.c_str());
  } catch (...) {
    ERROR("Unknown exception occured while exporting interface '%s'\n", 
	  iface.c_str());
  }
}

bool XMLRPC2DIServer::initialize() {
  DBG("Binding XMLRPC2DIServer to port %u \n", port);
  if (!s->bindAndListen(port, bind_ip)) {
    ERROR("Binding XMLRPC2DIServer to %s:%u\n", bind_ip.c_str(), port);
    return false;
  }
  return true;
}

void XMLRPC2DIServer::run() {

  // register us as SIP event receiver for MOD_NAME
  AmEventDispatcher::instance()->addEventQueue(MOD_NAME, this);

  DBG("starting XMLRPC2DIServer...\n");
  running.set(true);
  do {
    s->work(DEF_XMLRPCSERVER_WORK_INTERVAL);
    processEvents();
  } 
  while(running.get());

  AmEventDispatcher::instance()->delEventQueue(MOD_NAME);
  DBG("Exiting XMLRPC2DIServer.\n");
}

void XMLRPC2DIServer::process(AmEvent* ev) {
  if (ev->event_id == E_SYSTEM) {
    AmSystemEvent* sys_ev = dynamic_cast<AmSystemEvent*>(ev);
    if(sys_ev){	
      DBG("XMLRPC2DIServer received system Event\n");
      if (sys_ev->sys_event == AmSystemEvent::ServerShutdown) {
	DBG("XMLRPC2DIServer received system Event: ServerShutdown, "
	    "stopping thread\n");
	running.set(false);
      }
      return;
    }
  }
  WARN("unknown event received\n");
}

void XMLRPC2DIServer::on_stop() {
  DBG("on_stop().\n");
  running.set(false);
}

void XMLRPC2DIServerCallsMethod::execute(XmlRpcValue& params, XmlRpcValue& result) {
  int res = AmSession::getSessionNum();
  DBG("XMLRPC2DI: calls = %d\n", res);
  result = res;
}

void XMLRPC2DIServerGetLoglevelMethod::execute(XmlRpcValue& params, XmlRpcValue& result) {
  int res = log_level;
  DBG("XMLRPC2DI: get_loglevel returns %d\n", res);
  result = res;
}

void XMLRPC2DIServerSetLoglevelMethod::execute(XmlRpcValue& params, XmlRpcValue& result) {
  log_level = params[0];
  DBG("XMLRPC2DI: set log level to %d.\n", (int)params[0]);
  result = "200 OK";
}


void XMLRPC2DIServerGetShutdownmodeMethod::execute(XmlRpcValue& params, XmlRpcValue& result) {
  DBG("XMLRPC2DI: get_shutdownmode returns %s\n", AmConfig::ShutdownMode?"true":"false");
  result = (bool)AmConfig::ShutdownMode;
}

void XMLRPC2DIServerSetShutdownmodeMethod::execute(XmlRpcValue& params, XmlRpcValue& result) {
  AmConfig::ShutdownMode = params[0];
  DBG("XMLRPC2DI: set shutdownmode to %s.\n", AmConfig::ShutdownMode?"true":"false");
  result = "200 OK";
}

void XMLRPC2DIServerGetCPSLimitMethod::execute(XmlRpcValue& params, XmlRpcValue& result) {
  pair<unsigned int, unsigned int> l = AmSessionContainer::instance()->getCPSLimit();
  DBG("XMLRPC2DI: get_cpslimit returns %d and %d\n", l.first, l.second);
  result = int2str(l.first) + " " + int2str(l.second);
}

void XMLRPC2DIServerSetCPSLimitMethod::execute(XmlRpcValue& params, XmlRpcValue& result) {
  AmSessionContainer::instance()->setCPSLimit((int)params[0]);
  DBG("XMLRPC2DI: set cpslimit to %u.\n",
    AmSessionContainer::instance()->getCPSLimit().first);
  result = "200 OK";
}

void XMLRPC2DIServerGetCpsavgMethod::execute(XmlRpcValue& params, XmlRpcValue& result) {
  int l = AmSessionContainer::instance()->getAvgCPS();
  DBG("XMLRPC2DI: get_cpsavg returns %d\n", l);
  result = l;
}

void XMLRPC2DIServerGetCpsmaxMethod::execute(XmlRpcValue& params, XmlRpcValue& result) {
  int l = AmSessionContainer::instance()->getMaxCPS();
  DBG("XMLRPC2DI: get_cpsmax returns %d\n", l);
  result = l;
}

#define XMLMETH_EXEC(_meth, _sess_func, _descr)				\
  void _meth::execute(XmlRpcValue& params, XmlRpcValue& result) {	\
  unsigned int res = AmSession::_sess_func();				\
  result = (int)res;							\
  DBG("XMLRPC2DI: " _descr "(): %u\n", res);				\
}

XMLMETH_EXEC(XMLRPC2DIServerGetSessionCount, getSessionCount, "get_sessioncount");
XMLMETH_EXEC(XMLRPC2DIServerGetCallsavgMethod, getAvgSessionNum, "get_callsavg");
XMLMETH_EXEC(XMLRPC2DIServerGetCallsmaxMethod, getMaxSessionNum, "get_callsmax");
#undef XMLMETH_EXEC

void XMLRPC2DIServerDIMethod::execute(XmlRpcValue& params, XmlRpcValue& result) {
  try {
    if (params.size() < 2) {
      DBG("XMLRPC2DI: ERROR: need at least factory name"
	  " and function name to call\n");
      throw XmlRpcException("need at least factory name"
			    " and function name to call", 400);
    }
    
    string fact_name = params[0];
    string fct_name = params[1];

    DBG("XMLRPC2DI: factory '%s' function '%s'\n", 
	fact_name.c_str(), fct_name.c_str());

    // get args
    AmArg args;
    XMLRPC2DIServer::xmlrpcval2amargarray(params, args, 2);
  
    if (XMLRPC2DI::DebugServerParams) {
      DBG(" params: <%s>\n", AmArg::print(args).c_str()); 
    }

    AmDynInvokeFactory* di_f = AmPlugIn::instance()->getFactory4Di(fact_name);
    if(!di_f){
      throw XmlRpcException("could not get factory", 500);
    }
    AmDynInvoke* di = di_f->getInstance();
    if(!di){
      throw XmlRpcException("could not get instance from factory", 500);
    }
    AmArg ret;
    di->invoke(fct_name, args, ret);


    if (XMLRPC2DI::DebugServerResult) {
      DBG(" result: <%s>\n", AmArg::print(ret).c_str()); 
    }
  
    XMLRPC2DIServer::amarg2xmlrpcval(ret, result);


  } catch (const XmlRpcException& e) {
    throw;
  } catch (const AmDynInvoke::NotImplemented& e) {
    throw XmlRpcException("Exception: AmDynInvoke::NotImplemented: "
			  + e.what, 504);
  } catch (const AmArg::OutOfBoundsException& e) {
    throw XmlRpcException("Exception: AmArg out of bounds - parameter number mismatch.", 300);
  } catch (const AmArg::TypeMismatchException& e) {
    throw XmlRpcException("Exception: Type mismatch in arguments.", 300);
  } catch (const string& e) {
    throw XmlRpcException("Exception: "+e, 500);
  } catch (const std::exception& e) {
    throw XmlRpcException("Exception: " + string(e.what()), 500);
  } catch (...) {
    throw XmlRpcException("Exception occured.", 500);
  }
}


void XMLRPC2DIServer::xmlrpcval2amargarray(XmlRpcValue& v, AmArg& a, 
					   unsigned int start_index) {
  if (v.valid()) {
    a.assertArray();
    size_t a_array_pos = a.size();

    for (int i=start_index; i<v.size();i++) {
      xmlrpcval2amarg(v[i], a[a_array_pos]);
      a_array_pos++;
    }
  }
}
void XMLRPC2DIServer::xmlrpcval2amarg(XmlRpcValue& v, AmArg& a) {
  if (v.valid()) {
    switch (v.getType()) {
    case XmlRpcValue::TypeInt:   {  /* DBG("X->A INT\n"); */ a = (int)v;    }  break;
    case XmlRpcValue::TypeDouble:{  /*  DBG("X->A DBL\n"); */ a = (double)v; }  break;
    case XmlRpcValue::TypeString:{  /*  DBG("X->A STR\n"); */ a = ((string)v).c_str(); }  break;
    case XmlRpcValue::TypeBoolean : { /*   DBG("X->A BOL\n"); */ a = (bool)v;  } break;
    case XmlRpcValue::TypeInvalid : { /*   DBG("X->A Inv\n"); */  a = AmArg();  } break;
      
    case XmlRpcValue::TypeArray: { 
      // DBG("X->A ARR\n");
      a.assertArray();
      xmlrpcval2amargarray(v, a, 0);
    } break;
#ifdef XMLRPCPP_SUPPORT_STRUCT_ACCESS
    case XmlRpcValue::TypeStruct: {
       // DBG("X->A STR\n");
      a.assertStruct();
      const XmlRpc::XmlRpcValue::ValueStruct& xvs = 
	(XmlRpc::XmlRpcValue::ValueStruct)v;
      for (XmlRpc::XmlRpcValue::ValueStruct::const_iterator it=
	     xvs.begin(); it != xvs.end(); ++it) {
	// not nice but cast operators in XmlRpcValue are not const
	XmlRpcValue& var = const_cast<XmlRpcValue&>(it->second);
	a[it->first] = AmArg();
	xmlrpcval2amarg(var, a[it->first]);
      }      
    } break;
#endif

    case XmlRpcValue::TypeBase64: {
      ArgBlob ab;
      const XmlRpcValue::BinaryData& bd = v;
      ab.len = bd.size();
      ab.data = malloc(ab.len);
      int i = 0;
      for (XmlRpcValue::BinaryData::const_iterator it=
       bd.begin(); it != bd.end(); ++it) {
        ((char*)ab.data)[i] = *it;
        ++i;
      }
      a = ab;
    } break;

      // TODO: support more types (datetime, struct, ...)
    default:     throw XmlRpcException("unsupported parameter type", 400);
    };
  }
}

void XMLRPC2DIServer::amarg2xmlrpcval(const AmArg& a, 
				      XmlRpcValue& result) {
  switch (a.getType()) {

  case AmArg::Undef:
    result = 0; // XmlRpcValue();
    break;
  
  case AmArg::Bool:  
    result = a.asBool();
    break;

  case AmArg::CStr:  
    //    DBG("a->X CSTR\n");
    result = string(a.asCStr()); break;

  case AmArg::Int:
    //    DBG("a->X INT\n");  
    result=a.asInt(); break;

  case AmArg::Double: 
    //    DBG("a->X DOUBLE\n");  
    result=a.asDouble(); break;

  case AmArg::Array:
    //    DBG("a->X ARRAY size %u\n", a.size());  
    result.setSize(a.size());
    for (size_t i=0;i<a.size();i++) {
      // duh... recursion...
      amarg2xmlrpcval(a.get(i), result[i]);
    }
    break;

  case AmArg::Struct:
    //    DBG("a->X STRUCT size %u\n", a.size());  
    for (AmArg::ValueStruct::const_iterator it = 
	   a.begin(); it != a.end(); it++) {
      // duh... recursion...
      amarg2xmlrpcval(it->second, result[it->first]);
    }
    break;

  default: { WARN("unsupported return value type %d\n", a.getType()); } break;
    // TODO: do sth with the data here ?
  }
}

DIMethodProxy::DIMethodProxy(std::string const &server_method_name, 
			     std::string const &di_method_name, 
			     AmDynInvokeFactory* di_factory)
  : XmlRpcServerMethod(server_method_name),
    di_method_name(di_method_name),
    server_method_name(server_method_name),
    di_factory(di_factory)
{ }    
  
void DIMethodProxy::execute(XmlRpcValue& params, 
			    XmlRpcValue& result) {

  try {
    if (NULL == di_factory) {
      throw XmlRpcException("could not get DI factory", 500);
    }
  
    AmDynInvoke* di = di_factory->getInstance();
    if(NULL == di){
      throw XmlRpcException("could not get instance from factory", 500);
    }
    
    AmArg args, ret;

    
    DBG("XMLRPC2DI '%s': function '%s'\n", 
	server_method_name.c_str(),
	di_method_name.c_str());

    XMLRPC2DIServer::xmlrpcval2amarg(params, args);
    if (XMLRPC2DI::DebugServerParams) {
      DBG(" params: <%s>\n", AmArg::print(args).c_str()); 
    }

    di->invoke(di_method_name, args, ret);

    if (XMLRPC2DI::DebugServerResult) {
      DBG(" result: <%s>\n", AmArg::print(ret).c_str()); 
    }
    
    XMLRPC2DIServer::amarg2xmlrpcval(ret, result);

  } catch (const XmlRpcException& e) {
    throw;
  } catch (const AmDynInvoke::NotImplemented& e) {
    throw XmlRpcException("Exception: AmDynInvoke::NotImplemented: "
			  + e.what, 504);
  } catch (const AmArg::OutOfBoundsException& e) {
    throw XmlRpcException("Exception: AmArg out of bounds - parameter number mismatch.", 300);
  } catch (const AmArg::TypeMismatchException& e) {
    throw XmlRpcException("Exception: Type mismatch in arguments.", 300);
  } catch (const string& e) {
    throw XmlRpcException("Exception: "+e, 500);
  } catch (const std::exception& e) {
    throw XmlRpcException("Exception: " + string(e.what()), 500);
  } catch (...) {
    throw XmlRpcException("Exception occured.", 500);
  }
}
