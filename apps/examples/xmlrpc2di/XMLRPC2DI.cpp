/*
 * $Id: XMLRPC2DI.cpp 145 2006-11-26 00:01:18Z sayer $
 *
 * Copyright (C) 2007 iptego GmbH
 *
 * This file is part of sems, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * For a license to use the sems software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * sems is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "XMLRPC2DI.h"

#include "AmSessionContainer.h"
#include "AmPlugIn.h"
#include "log.h"
#include "AmConfigReader.h"
#include "AmUtils.h"
#include "AmArg.h"

#define MOD_NAME "xmlrpc2di"

#define XMLRPC_PORT   "8090" // default port
EXPORT_PLUGIN_CLASS_FACTORY(XMLRPC2DI, MOD_NAME)

  XMLRPC2DI::XMLRPC2DI(string mod_name) 
    : AmDynInvokeFactory(mod_name)
{
}

int XMLRPC2DI::onLoad() {

  AmConfigReader cfg;
  if(cfg.loadFile(AmConfig::ModConfigPath + string(MOD_NAME ".conf")))
    return -1;

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
  server = new XMLRPC2DIServer(XMLRPCPort);
  server->start();
  return 0;
}

// XMLRPC server functions

XMLRPC2DIServer::XMLRPC2DIServer(unsigned int port) 
  : port(port),
    calls_method(&s),
    setloglevel_method(&s),
    getloglevel_method(&s),
    di_method(&s)
{	
  DBG("Initialized XMLRPC2DIServer with: \n");
  DBG("                          port = %u\n", port);
}

void XMLRPC2DIServer::run() {
  DBG("Binding XMLRPC2DIServer to port %u \n", port);
  s.bindAndListen(port);
  DBG("starting XMLRPC2DIServer...\n");
  s.work(-1.0);
}
void XMLRPC2DIServer::on_stop() {
  DBG("sorry, don't know how to stop the server.\n");
}

void XMLRPC2DIServerCallsMethod::execute(XmlRpcValue& params, XmlRpcValue& result) {
  int res = AmSessionContainer::instance()->getSize();
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


void XMLRPC2DIServerDIMethod::execute(XmlRpcValue& params, XmlRpcValue& result) {
  try {
    if (params.size() < 2) {
      DBG("XMLRPC2DI: ERROR: need at least factory name and function name to call\n");
      throw XmlRpcException("need at least factory name and function name to call", 400);
    }
    
    string fact_name = params[0];
    string fct_name = params[1];

    DBG("XMLRPC2DI: factory '%s' function '%s'\n", 
	fact_name.c_str(), fct_name.c_str());

    // get args
    AmArgArray args;
    for (int i=2; i<params.size();i++) {
      switch (params[i].getType()) {
      case XmlRpcValue::TypeInt:   { args.push(AmArg((int)params[i]));    }  break;
      case XmlRpcValue::TypeDouble:{ args.push(AmArg((double)params[i])); }  break;
      case XmlRpcValue::TypeString:{ args.push(AmArg(((string)params[i]).c_str())); }  break;
	// TODO: support more types (datetime, struct, ...)
      default:     throw XmlRpcException("unsupported parameter type", 400);
      };
    }
  
    AmDynInvokeFactory* di_f = AmPlugIn::instance()->getFactory4Di(fact_name);
    if(!di_f){
      throw XmlRpcException("could not get factory", 500);
    }
    AmDynInvoke* di = di_f->getInstance();
    if(!di){
      throw XmlRpcException("could not get instance from factory", 500);
    }
    AmArgArray ret;
    di->invoke(fct_name, args, ret);
  
    if (ret.size()) {
      result.setSize(ret.size());
    
      for (unsigned int i=0;i<ret.size();i++) {
	const AmArg& r = ret.get(i);
	switch (r.getType()) {
	case AmArg::CStr:  
	  result[i]= string(r.asCStr()); break;
	case AmArg::Int:  
	  result[i]=r.asInt(); break;
	case AmArg::Double: 
	  result[i]=r.asDouble(); break;
	default: break;
	  // TODO: do sth with the data here
	}
      }
    }
  } catch (const XmlRpcException& e) {
    throw;
  } catch (const AmDynInvoke::NotImplemented& e) {
    throw XmlRpcException("Exception: AmDynInvoke::NotImplemented: "
			  + e.what, 504);
  } catch (const AmArgArray::OutOfBoundsException& e) {
    throw XmlRpcException("Exception: AmArgArray out of bounds - paramter number mismatch.", 300);
  } catch (const string& e) {
    throw XmlRpcException("Exception: "+e, 500);
  } catch (...) {
    throw XmlRpcException("Exception occured.", 500);
  }
}
