/*
 * $Id$
 *
 * Copyright (C) 2008 iptego GmbH
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the SEMS software under conditions
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
#include "DSM.h"
#include "AmConfig.h"
#include "AmUtils.h"
#include "AmPlugIn.h"
#include "log.h"
#include "AmConfigReader.h"
#include "AmSessionContainer.h"
#include "ampi/MonitoringAPI.h"
#include "AmUriParser.h"
#include "DSMDialog.h"
#include "DSMChartReader.h"

#include <string>
#include <fstream>

#define MOD_NAME "dsm"

extern "C" void* plugin_class_create()
{
  return DSMFactory::instance();
}

DSMFactory* DSMFactory::_instance=0;

DSMFactory* DSMFactory::instance()
{
  if(_instance == NULL)
    _instance = new DSMFactory(MOD_NAME); 
  return _instance;
}

string DSMFactory::InboundStartDiag;
string DSMFactory::OutboundStartDiag;
bool DSMFactory::MonSelectUseCaller = true;
bool DSMFactory::MonSelectUseCallee = true;

map<string, string> DSMFactory::config;
bool DSMFactory::RunInviteEvent;
bool DSMFactory::SetParamVariables;

DSMFactory::DSMFactory(const string& _app_name)
  : AmSessionFactory(_app_name),AmDynInvokeFactory(_app_name),
    loaded(false)
{
}

DSMFactory::~DSMFactory() {
  for (map<string, AmPromptCollection*>::iterator it=
	 prompt_sets.begin(); it != prompt_sets.end(); it++)
    delete it->second;
}

int DSMFactory::onLoad()
{
  if (loaded)
    return 0;
  loaded = true;

  AmConfigReader cfg;
  if(cfg.loadFile(AmConfig::ModConfigPath + string(MOD_NAME ".conf")))
    return -1;
 
  // get application specific global parameters
  configureModule(cfg);
  
  vector<string> prompts_files = 
    explode(cfg.getParameter("load_prompts"), ",");
  for (vector<string>::iterator it=
	 prompts_files.begin(); it != prompts_files.end(); it++) {
    DBG("loading prompts from '%s'\n", it->c_str());
    std::ifstream ifs(it->c_str());
    string s;
    while (ifs.good() && !ifs.eof()) {
      getline(ifs, s);
      if (s.length() && s.find_first_not_of(" \t")!= string::npos &&
	  s[s.find_first_not_of(" \t")] != '#') {
        vector<string> p=explode(s, "=");
	if (p.size()==2) {
	  prompts.setPrompt(p[0], p[1], MOD_NAME);
	  DBG("added prompt '%s' as '%s'\n", 
	      p[0].c_str(), p[1].c_str());
	}
      }
    }
  }

  bool has_all_prompts = true;
  vector<string> required_prompts = 
    explode(cfg.getParameter("required_prompts"), ",");
  
  for (vector<string>::iterator it=required_prompts.begin(); 
       it != required_prompts.end(); it++) {
    if (!prompts.hasPrompt(*it)) {
      ERROR("required prompt '%s' not loaded.\n",
	    it->c_str());
      has_all_prompts = false;
    }
  }
  if (!has_all_prompts)
    return -1;

  string prompt_sets_path = cfg.getParameter("prompts_sets_path");

  vector<string> prompt_sets_names = 
    explode(cfg.getParameter("load_prompts_sets"), ",");
  for (vector<string>::iterator it=
	 prompt_sets_names.begin(); it != prompt_sets_names.end(); it++) {
    string fname = prompt_sets_path.empty() ? "": prompt_sets_path + "/";
    fname += *it;
    DBG("loading prompts for '%s' (file '%s')\n", it->c_str(), fname.c_str());
    std::ifstream ifs(fname.c_str());
    string s;
    if (!ifs.good()) {
      WARN("prompts set file '%s' could not be read\n", fname.c_str());
    }
    AmPromptCollection* pc = new AmPromptCollection();
    while (ifs.good() && !ifs.eof()) {
      getline(ifs, s);
      if (s.length() && s.find_first_not_of(" \t")!= string::npos &&
	  s[s.find_first_not_of(" \t")] != '#') {
        vector<string> p=explode(s, "=");
	if (p.size()==2) {
	  pc->setPrompt(p[0], p[1], MOD_NAME);
	  DBG("set '%s' added prompt '%s' as '%s'\n", 
	      it->c_str(), p[0].c_str(), p[1].c_str());
	}
      }
    }
    prompt_sets[*it] = pc;
  }

  string DiagPath = cfg.getParameter("diag_path");
  if (DiagPath.length() && DiagPath[DiagPath.length()-1] != '/')
    DiagPath += '/';

  string ModPath = cfg.getParameter("mod_path");

  string preload_mods = cfg.getParameter("preload_mods");
  vector<string> preload_names = explode(preload_mods, ",");
  if (preload_names.size()) {
    DSMChartReader reader;
    for (vector<string>::iterator it=
	   preload_names.begin(); it != preload_names.end(); it++) {
      DBG("preloading '%s'...\n", it->c_str());
      if (!reader.importModule("import("+*it+")", ModPath)) {
	ERROR("importing module '%s' for preload\n", it->c_str());
	return -1;
      }
      DSMModule* last_loaded = reader.mods.back();
      if (last_loaded) {
 	if (last_loaded->preload()) {
 	  DBG("Error while preloading '%s'\n", it->c_str());
 	  return -1;
 	}
      }
    }
  }
  // todo: pass preloaded mods to chart reader

  string LoadDiags = cfg.getParameter("load_diags");
  vector<string> diags_names = explode(LoadDiags, ",");
  for (vector<string>::iterator it=
	 diags_names.begin(); it != diags_names.end(); it++) {
    if (!diags.loadFile(DiagPath+*it+".dsm", *it, ModPath)) {
      ERROR("loading %s from %s\n", 
	    it->c_str(), (DiagPath+*it+".dsm").c_str());
      return -1;
    }
  }
  
  string RegisterDiags = cfg.getParameter("register_apps");
  vector<string> register_names = explode(RegisterDiags, ",");
  for (vector<string>::iterator it=
	 register_names.begin(); it != register_names.end(); it++) {
    if (diags.hasDiagram(*it)) {
      bool res = AmPlugIn::instance()->registerFactory4App(*it,this);
      if(res)
	INFO("DSM state machine registered: %s.\n",
	     it->c_str());
    } else {
      ERROR("trying to register application '%s' which is not loaded.\n",
	    it->c_str());
      return -1;
    }
  }

  InboundStartDiag = cfg.getParameter("inbound_start_diag");
  if (InboundStartDiag.empty()) {
    INFO("no 'inbound_start_diag' set in config. inbound calls disabled.\n");
  }
  OutboundStartDiag = cfg.getParameter("outbound_start_diag");
  if (OutboundStartDiag.empty()) {
    INFO("no 'outbound_start_diag' set in config. outbound calls disabled.\n");
  }

  if (!InboundStartDiag.empty() || OutboundStartDiag.empty())
    AmPlugIn::instance()->registerFactory4App("dsm",this);

  for (std::map<string,string>::const_iterator it = 
	 cfg.begin(); it != cfg.end(); it++) 
    config[it->first] = it->second;

  RunInviteEvent = cfg.getParameter("run_invite_event")=="yes";

  SetParamVariables = cfg.getParameter("set_param_variables")=="yes";

  if (cfg.getParameter("monitor_select_use_caller")=="no")
    MonSelectUseCaller = false;

  if (cfg.getParameter("monitor_select_use_callee")=="no")
    MonSelectUseCallee = false;

  return 0;
}

void DSMFactory::prepareSession(DSMDialog* s) {
  s->setPromptSets(prompt_sets);
}

void DSMFactory::addVariables(DSMDialog* s, const string& prefix,
			      map<string, string>& vars) {
  for (map<string, string>::iterator it = 
	 vars.begin(); it != vars.end(); it++) 
    s->var[prefix+it->first] = it->second;
}

void DSMFactory::addParams(DSMDialog* s, const string& hdrs) {
  // TODO: use real parser with quoting and optimize
  map<string, string> params;
  vector<string> items = explode(getHeader(hdrs, PARAM_HDR), ";");
  for (vector<string>::iterator it=items.begin(); 
       it != items.end(); it++) {
    vector<string> kv = explode(*it, "=");
    if (kv.size()==2) 
      params.insert(make_pair(kv[0], kv[1]));
  }
  addVariables(s, "", params);  
}

void AmArg2DSMStrMap(const AmArg& arg,
		     map<string, string>& vars) {
  for (AmArg::ValueStruct::const_iterator it=arg.begin(); 
       it != arg.end(); it++) {
    if (it->second.getType() == AmArg::CStr)
      vars[it->first] = it->second.asCStr();
    else if (it->second.getType() == AmArg::Array) {
      vars[it->first+"_size"] = int2str(it->second.size());
      for (size_t i=0;i<it->second.size();i++) {
	if (it->second.get(i).getType() == AmArg::CStr)
	  vars[it->first+"_"+int2str(i)] = it->second.get(i).asCStr();
	else
	  vars[it->first+"_"+int2str(i)] = AmArg::print(it->second.get(i));
      }
    } else {
      vars[it->first] = AmArg::print(it->second);	
    }
  }
}

AmSession* DSMFactory::onInvite(const AmSipRequest& req)
{
  string start_diag;
  map<string, string> vars;

  if (req.cmd == MOD_NAME) {
    if (InboundStartDiag.empty()) {
      ERROR("no inbound calls allowed\n");
      throw AmSession::Exception(488, "Not Acceptable Here");
    }
    if (InboundStartDiag=="$(mon_select)") {
#ifdef USE_MONITORING

      AmArg di_args, ret;
      if (MonSelectUseCaller) {
	AmUriParser from_parser;
	from_parser.uri = req.from_uri;
	if (!from_parser.parse_uri()) {
	  DBG("failed to parse from_uri '%s'\n", req.from_uri.c_str());
	  throw AmSession::Exception(488, "Not Acceptable Here");
	}
	
	if (NULL == MONITORING_GLOBAL_INTERFACE) {
	  ERROR("using $(mon_select) but monitoring not loaded\n");
	  throw AmSession::Exception(488, "Not Acceptable Here");
	}

	AmArg caller_filter;
	caller_filter.push("caller");
	caller_filter.push(from_parser.uri_user);
	DBG(" && looking for caller=='%s'\n", from_parser.uri_user.c_str());
	di_args.push(caller_filter);
      }
      if (MonSelectUseCallee) {
	AmArg callee_filter;
	callee_filter.push("callee");
	callee_filter.push(req.user);
	DBG(" && looking for callee=='%s'\n", req.user.c_str());
	di_args.push(callee_filter);	
      }
      MONITORING_GLOBAL_INTERFACE->invoke("listByFilter",di_args,ret);
      
      if ((ret.getType()!=AmArg::Array)||
	  !ret.size()) {
	INFO("call info not found. caller uri %s, r-uri %s\n", 
	     req.from_uri.c_str(), req.r_uri.c_str());
	throw AmSession::Exception(488, "Not Acceptable Here");
      }

      AmArg sess_id, sess_params;
      if (ret.size()>1) {
	DBG("multiple call info found - picking the first one\n");
      }
      sess_id.push(ret.get(0).asCStr());
      MONITORING_GLOBAL_INTERFACE->invoke("get",sess_id,sess_params);
      
      if ((sess_params.getType()!=AmArg::Array)||
	  !sess_params.size() ||
	  sess_params.get(0).getType() != AmArg::Struct) {
	INFO("call parameters not found. caller uri %s, r-uri %s, id %s\n", 
	     req.from_uri.c_str(), req.r_uri.c_str(), ret.get(0).asCStr());
	throw AmSession::Exception(488, "Not Acceptable Here");
      }

      AmArg& sess_dict = sess_params.get(0);
      if (sess_dict.hasMember("app")) {
	start_diag = sess_dict["app"].asCStr();
	DBG("selected application '%s' for session\n", start_diag.c_str());
      } else {
	ERROR("selected session params don't contain 'app'\n");
	throw AmSession::Exception(488, "Not Acceptable Here");
      }
      AmArg2DSMStrMap(sess_dict["appParams"], vars);
	
#else
      ERROR("using $(mon_select) for dsm application, but compiled without monitoring support!\n");
      throw AmSession::Exception(488, "Not Acceptable Here");
#endif
    } else {
      start_diag = InboundStartDiag;
    }
  } else {
    start_diag = req.cmd;
  }
  DSMDialog* s = new DSMDialog(&prompts, diags, start_diag, NULL);
  prepareSession(s);
  addVariables(s, "config.", config);

  if (SetParamVariables) 
    addParams(s, req.hdrs);


  if (!vars.empty())
    addVariables(s, "", vars);

  return s;
}

// outgoing call
AmSession* DSMFactory::onInvite(const AmSipRequest& req,
				AmArg& session_params) 
{

  string start_diag;

  if (req.cmd == MOD_NAME) {
    if (OutboundStartDiag.empty()) {
      ERROR("no outbound calls allowed\n");
    throw AmSession::Exception(488, "Not Acceptable Here");
    }
  } else {
    start_diag = req.cmd;
  }

  UACAuthCred* cred = NULL;
  map<string, string> vars;
  // Creds
  if (session_params.getType() == AmArg::AObject) {
    ArgObject* cred_obj = session_params.asObject();
    if (cred_obj)
      cred = dynamic_cast<UACAuthCred*>(cred_obj);
  } else if (session_params.getType() == AmArg::Array) {
    DBG("session params is array - size %d\n", session_params.size());
    // Creds
    if (session_params.get(0).getType() == AmArg::AObject) {
      ArgObject* cred_obj = session_params.get(0).asObject();
      if (cred_obj)
	cred = dynamic_cast<UACAuthCred*>(cred_obj);
    }
    // Creds + vars
    if (session_params.size()>1 && 
	session_params.get(1).getType() == AmArg::Struct) {
      AmArg2DSMStrMap(session_params.get(1), vars);
    }
  } else if (session_params.getType() == AmArg::Struct) {
    // vars
    AmArg2DSMStrMap(session_params, vars);
  }

  DSMDialog* s = new DSMDialog(&prompts, diags, start_diag, cred); 
  prepareSession(s);  

  addVariables(s, "config.", config);
  if (!vars.empty())
    addVariables(s, "", vars);

  if (SetParamVariables) 
    addParams(s, req.hdrs); 

  if (NULL == cred) {
    WARN("discarding unknown session parameters.\n");
  } else {
    AmSessionEventHandlerFactory* uac_auth_f = 
      AmPlugIn::instance()->getFactory4Seh("uac_auth");
    if (uac_auth_f != NULL) {
      DBG("UAC Auth enabled for new DSM session.\n");
      AmSessionEventHandler* h = uac_auth_f->getHandler(s);
      if (h != NULL )
	s->addHandler(h);
    } else {
      ERROR("uac_auth interface not accessible. "
	    "Load uac_auth for authenticated dialout.\n");
    }		
  }

  return s;
}


void DSMFactory::invoke(const string& method, const AmArg& args, 
				AmArg& ret)
{
  if(method == "postDSMEvent"){
    assertArgCStr(args.get(0))

    DSMEvent* ev = new DSMEvent();
    for (size_t i=0;i<args[1].size();i++)
      ev->params[args[1][i][0].asCStr()] = args[1][i][1].asCStr();

    if (AmSessionContainer::instance()->postEvent(args.get(0).asCStr(), ev)) {
      ret.push(AmArg(200));
      ret.push(AmArg("OK"));
    } else {
      ret.push(AmArg(404));
      ret.push(AmArg("Session not found"));
    }
      
  } else if(method == "_list"){ 
    ret.push(AmArg("postDSMEvent"));
  }  else
    throw AmDynInvoke::NotImplemented(method);
}
