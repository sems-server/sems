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
#include "DSMCall.h"
#include "DSMChartReader.h"
#include "AmSipHeaders.h"
#include "AmEventDispatcher.h"

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

bool DSMFactory::DebugDSM;
string DSMFactory::InboundStartDiag;
string DSMFactory::OutboundStartDiag;

#ifdef USE_MONITORING
bool DSMFactory::MonitoringFullCallgraph;
bool DSMFactory::MonitoringFullTransitions;

MonSelectType DSMFactory::MonSelectCaller;
MonSelectType DSMFactory::MonSelectCallee;
#endif // USE_MONITORING

map<string, string> DSMFactory::config;
bool DSMFactory::RunInviteEvent;
bool DSMFactory::SetParamVariables;

DSMFactory::DSMFactory(const string& _app_name)
  : AmSessionFactory(_app_name),AmDynInvokeFactory(_app_name),
    loaded(false)
{
  AmEventDispatcher::instance()->addEventQueue("dsm", this);

  diags = new DSMStateDiagramCollection();
}

void DSMFactory::postEvent(AmEvent* e) {
  AmSystemEvent* sys_ev = dynamic_cast<AmSystemEvent*>(e);  
  if (sys_ev && 
      sys_ev->sys_event == AmSystemEvent::ServerShutdown) {
    DBG("stopping DSM...\n");
    preload_reader.cleanup();
    AmEventDispatcher::instance()->delEventQueue("dsm");
    return;
  }

  WARN("received unknown event\n");
}

DSMFactory::~DSMFactory() {
  for (map<string, AmPromptCollection*>::iterator it=
	 prompt_sets.begin(); it != prompt_sets.end(); it++)
    delete it->second;

  for (vector<DSMStateDiagramCollection*>::iterator it=
	 old_diags.begin(); it != old_diags.end(); it++)
    delete *it;
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

  DebugDSM = cfg.getParameter("debug_raw_dsm") == "yes";
 
  string DiagPath = cfg.getParameter("diag_path");
  if (DiagPath.length() && DiagPath[DiagPath.length()-1] != '/')
    DiagPath += '/';

  string ModPath = cfg.getParameter("mod_path");

  string err;
  int res = preloadModules(cfg, err, ModPath);
  if (res<0) {
    ERROR("%s\n", err.c_str());
    return res;
  }

  // todo: pass preloaded mods to chart reader

  string LoadDiags = cfg.getParameter("load_diags");
  vector<string> diags_names = explode(LoadDiags, ",");
  for (vector<string>::iterator it=
	 diags_names.begin(); it != diags_names.end(); it++) {
    if (!diags->loadFile(DiagPath+*it+".dsm", *it, ModPath, DebugDSM)) {
      ERROR("loading %s from %s\n", 
	    it->c_str(), (DiagPath+*it+".dsm").c_str());
      return -1;
    }
  }
  
  string RegisterDiags = cfg.getParameter("register_apps");
  vector<string> register_names = explode(RegisterDiags, ",");
  for (vector<string>::iterator it=
	 register_names.begin(); it != register_names.end(); it++) {
    if (diags->hasDiagram(*it)) {
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
  if (InboundStartDiag.empty() && register_names.empty()) {
    INFO("no 'inbound_start_diag' set in config. inbound calls with application 'dsm' disabled.\n");
  }
  OutboundStartDiag = cfg.getParameter("outbound_start_diag");
  if (OutboundStartDiag.empty() && register_names.empty()) {
    INFO("no 'outbound_start_diag' set in config. outbound calls with application 'dsm' disabled.\n");
  }

  if (!InboundStartDiag.empty() || OutboundStartDiag.empty())
    AmPlugIn::instance()->registerFactory4App("dsm",this);

  for (std::map<string,string>::const_iterator it = 
	 cfg.begin(); it != cfg.end(); it++) 
    config[it->first] = it->second;

  RunInviteEvent = cfg.getParameter("run_invite_event")=="yes";

  SetParamVariables = cfg.getParameter("set_param_variables")=="yes";

#ifdef USE_MONITORING
  string monitoring_full_callgraph = cfg.getParameter("monitoring_full_stategraph");
  MonitoringFullCallgraph = monitoring_full_callgraph == "yes";
  DBG("%sogging full call graph (states) to monitoring.\n",
      MonitoringFullCallgraph?"L":"Not l");

  string monitoring_full_transitions = cfg.getParameter("monitoring_full_transitions");
  MonitoringFullTransitions = monitoring_full_transitions == "yes";
  DBG("%sogging full call graph (transitions) to monitoring.\n",
      MonitoringFullTransitions?"L":"Not l");

  string cfg_usecaller = cfg.getParameter("monitor_select_use_caller");
  if (cfg_usecaller.empty() || cfg_usecaller=="from") 
    MonSelectCaller = MonSelect_FROM;
  else if (cfg_usecaller=="no") 
    MonSelectCaller = MonSelect_NONE;
  else if (cfg_usecaller=="pai") 
    MonSelectCaller = MonSelect_PAI;
  else {
    ERROR("monitor_select_use_caller value '%s' not understood\n",
	  cfg_usecaller.c_str());
  }

  string cfg_usecallee = cfg.getParameter("monitor_select_use_callee");
  if (cfg_usecallee.empty() || cfg_usecallee=="ruri") 
    MonSelectCallee = MonSelect_RURI;
  else if (cfg_usecallee=="no") 
    MonSelectCallee = MonSelect_NONE;
  else if (cfg_usecallee=="from") 
    MonSelectCallee = MonSelect_FROM;
  else {
    ERROR("monitor_select_use_callee value '%s' not understood\n",
	  cfg_usecallee.c_str());
  }
#endif

  return 0;
}

void DSMFactory::prepareSession(DSMCall* s) {
  s->setPromptSets(prompt_sets);
}

void DSMFactory::addVariables(DSMCall* s, const string& prefix,
			      map<string, string>& vars) {
  for (map<string, string>::iterator it = 
	 vars.begin(); it != vars.end(); it++) 
    s->var[prefix+it->first] = it->second;
}

void DSMFactory::addParams(DSMCall* s, const string& hdrs) {
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

      if (NULL == MONITORING_GLOBAL_INTERFACE) {
	ERROR("using $(mon_select) but monitoring not loaded\n");
	throw AmSession::Exception(488, "Not Acceptable Here");
      }

      AmArg di_args, ret;
      if (MonSelectCaller != MonSelect_NONE) {
	AmUriParser from_parser;
	if (MonSelectCaller == MonSelect_FROM)
	  from_parser.uri = req.from_uri;
	else {
	  size_t end;
	  string pai = getHeader(req.hdrs, SIP_HDR_P_ASSERTED_IDENTITY);
	  if (!from_parser.parse_contact(pai, 0, end)) {
	    ERROR("Failed to parse "SIP_HDR_P_ASSERTED_IDENTITY " '%s'\n",
		  pai.c_str());
	    throw AmSession::Exception(488, "Not Acceptable Here");
	  }
	}

	if (!from_parser.parse_uri()) {
	  DBG("failed to parse caller uri '%s'\n", from_parser.uri.c_str());
	  throw AmSession::Exception(488, "Not Acceptable Here");
	}
	
	AmArg caller_filter;
	caller_filter.push("caller");
	caller_filter.push(from_parser.uri_user);
	DBG(" && looking for caller=='%s'\n", from_parser.uri_user.c_str());
	di_args.push(caller_filter);
      }


      if (MonSelectCallee != MonSelect_NONE) {
	AmArg callee_filter;
	callee_filter.push("callee");
	if (MonSelectCallee == MonSelect_RURI)
	  callee_filter.push(req.user);
	else {
	  AmUriParser to_parser;
	  size_t end;
	  if (!to_parser.parse_contact(req.to, 0, end)) {
	    ERROR("Failed to parse To '%s'\n", req.to.c_str());
	    throw AmSession::Exception(488, "Not Acceptable Here");
	  }
	  if (!to_parser.parse_uri()) {
	    DBG("failed to parse callee uri '%s'\n", to_parser.uri.c_str());
	    throw AmSession::Exception(488, "Not Acceptable Here");
	  }
	  callee_filter.push(to_parser.uri_user);
	}
	  
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
      const char* session_id = ret.get(0).asCStr();
      sess_id.push(session_id);
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
      vars["mon_session_record"] = session_id;
	
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
  diags_mut.lock();
  DSMCall* s = new DSMCall(&prompts, *diags, start_diag, NULL);
  diags_mut.unlock();

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

  DSMCall* s = new DSMCall(&prompts, *diags, start_diag, cred); 
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

void DSMFactory::reloadDSMs(const AmArg& args, AmArg& ret) {
  DSMStateDiagramCollection* new_diags = new DSMStateDiagramCollection();

  AmConfigReader cfg;
  if(cfg.loadFile(AmConfig::ModConfigPath + string(MOD_NAME ".conf"))) {
      ret.push(500);
      ret.push("loading config file " +AmConfig::ModConfigPath + string(MOD_NAME ".conf"));
      return ;
  }

  string DiagPath = cfg.getParameter("diag_path");
  if (DiagPath.length() && DiagPath[DiagPath.length()-1] != '/')
    DiagPath += '/';

  string ModPath = cfg.getParameter("mod_path");

  string LoadDiags = cfg.getParameter("load_diags");
  vector<string> diags_names = explode(LoadDiags, ",");
  for (vector<string>::iterator it=
	 diags_names.begin(); it != diags_names.end(); it++) {
    if (!new_diags->loadFile(DiagPath+*it+".dsm", *it, ModPath, DebugDSM)) {
      ERROR("loading %s from %s\n", 
	    it->c_str(), (DiagPath+*it+".dsm").c_str());
      ret.push(500);
      ret.push("loading " +*it+ " from "+ DiagPath+*it+".dsm");
      return;
    }
  }
  diags_mut.lock();
  old_diags.push_back(diags);
  diags = new_diags; 
  diags_mut.unlock();

  ret.push(200);
  ret.push("DSMs reloaded");
}


int DSMFactory::preloadModules(AmConfigReader& cfg, string& res, const string& ModPath) {
  string preload_mods = cfg.getParameter("preload_mods");
  vector<string> preload_names = explode(preload_mods, ",");
  if (preload_names.size()) {
    for (vector<string>::iterator it=
	   preload_names.begin(); it != preload_names.end(); it++) {
      DBG("preloading '%s'...\n", it->c_str());
      if (!preload_reader.importModule("import("+*it+")", ModPath)) {
	res = "importing module '"+*it+"' for preload\n";
	return -1;
      }
      DSMModule* last_loaded = preload_reader.mods.back();
      if (last_loaded) {
 	if (last_loaded->preload()) {
	  res = "Error while preloading '"+*it+"'\n";
 	  return -1;
 	}
      }
    }
  }

  return 0;
}

void DSMFactory::preloadModules(const AmArg& args, AmArg& ret) {
  AmConfigReader cfg;
  if(cfg.loadFile(AmConfig::ModConfigPath + string(MOD_NAME ".conf"))) {
      ret.push(500);
      ret.push("loading config file " +AmConfig::ModConfigPath + string(MOD_NAME ".conf"));
      return ;
  }
  string err;

  string ModPath = cfg.getParameter("mod_path");

  int res = preloadModules(cfg, err, ModPath);
  if (res<0) {
    ret.push(500);
    ret.push(err);
  } else {
    ret.push(200);
    ret.push("modules preloaded");
  }
}

void DSMFactory::preloadModule(const AmArg& args, AmArg& ret) {
  string mod_name = args.get(0).asCStr();
  string mod_path = args.get(1).asCStr();

  if (!preload_reader.importModule("import("+mod_name+")", mod_path)) {
    ret.push(500);
    ret.push("importing module '"+mod_name+"' for preload");
    return;
  }
  DSMModule* last_loaded = preload_reader.mods.back();
  if (last_loaded) {
    if (last_loaded->preload()) {
      ret.push(500);
      ret.push("Error while preloading '"+mod_name+"'");
      return;
    }
  }
  ret.push(200);
  ret.push("module preloaded.");
  return;
}

void DSMFactory::listDSMs(const AmArg& args, AmArg& ret) {
  diags_mut.lock();
  vector<string> names = diags->getDiagramNames();
  diags_mut.unlock();
  for (vector<string>::iterator it=
	 names.begin(); it != names.end(); it++) {
    ret.push(*it);
  }
}

void DSMFactory::hasDSM(const AmArg& args, AmArg& ret) {
  diags_mut.lock();
  bool res = diags->hasDiagram(args.get(0).asCStr());
  diags_mut.unlock();
  if (res)
    ret.push("1");
  else
    ret.push("0");
}

void DSMFactory::registerApplication(const AmArg& args, AmArg& ret) {
  string diag_name = args.get(0).asCStr();
  diags_mut.lock();
  bool has_diag = diags->hasDiagram(diag_name);
  diags_mut.unlock();  
  if (!has_diag) {
    ret.push(400);
    ret.push("unknown application (DSM)");
    return;
  }

  bool res = AmPlugIn::instance()->registerFactory4App(diag_name,this);
  if(res) {
    INFO("DSM state machine registered: %s.\n",diag_name.c_str());
    ret.push(200);
    ret.push("registered DSM application");    
  } else {
    ret.push(500);
    ret.push("Error registering DSM application (already registered?)");
  }
}

void DSMFactory::loadDSM(const AmArg& args, AmArg& ret) {
  string dsm_name  = args.get(0).asCStr();

  AmConfigReader cfg;
  if(cfg.loadFile(AmConfig::ModConfigPath + string(MOD_NAME ".conf"))) {
      ret.push(500);
      ret.push("loading config file " +AmConfig::ModConfigPath + string(MOD_NAME ".conf"));
      return;
  }

  string DiagPath = cfg.getParameter("diag_path");
  if (DiagPath.length() && DiagPath[DiagPath.length()-1] != '/')
    DiagPath += '/';

  string ModPath = cfg.getParameter("mod_path");

  string dsm_file_name = DiagPath+dsm_name+".dsm";
  string res = "OK";
  diags_mut.lock();
  if (diags->hasDiagram(dsm_name)) {
    ret.push(400);
    ret.push("DSM named '" + dsm_name + "' already loaded (use reloadDSMs to reload all)");
  } else {
    if (!diags->loadFile(dsm_file_name, dsm_name, ModPath, DebugDSM)) {
      ret.push(500);
      ret.push("error loading "+dsm_name+" from "+ dsm_file_name);
    } else {
      ret.push(200);
      ret.push("loaded "+dsm_name+" from "+ dsm_file_name);
    }
  }
  diags_mut.unlock();
}

void DSMFactory::loadDSMWithPaths(const AmArg& args, AmArg& ret) {
  string dsm_name  = args.get(0).asCStr();
  string diag_path = args.get(1).asCStr();
  string mod_path  = args.get(2).asCStr();

  string res = "OK";
  diags_mut.lock();
  if (diags->hasDiagram(dsm_name)) {
    ret.push(400);
    ret.push("DSM named '" + dsm_name + "' already loaded (use reloadDSMs to reload all)");
  } else {
    if (!diags->loadFile(diag_path+dsm_name+".dsm", dsm_name, mod_path, DebugDSM)) {
      ret.push(500);
      ret.push("error loading "+dsm_name+" from "+ diag_path+dsm_name+".dsm");
    } else {
      ret.push(200);
      ret.push("loaded "+dsm_name+" from "+ diag_path+dsm_name+".dsm");
    }
  }
  diags_mut.unlock();
}

void DSMFactory::invoke(const string& method, const AmArg& args, 
				AmArg& ret)
{
  if (method == "postDSMEvent"){
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
  } else if (method == "reloadDSMs"){
    reloadDSMs(args,ret);
  } else if (method == "loadDSM"){
    args.assertArrayFmt("s");
    loadDSM(args,ret);
  } else if (method == "loadDSMWithPath"){
    args.assertArrayFmt("sss");
    loadDSMWithPaths(args,ret);
  } else if (method == "preloadModules"){
    preloadModules(args,ret);
  } else if (method == "preloadModule"){
    args.assertArrayFmt("ss");
    preloadModule(args,ret);
  } else if (method == "hasDSM"){
    args.assertArrayFmt("s");
    hasDSM(args,ret);      
  } else if (method == "listDSMs"){
    listDSMs(args,ret);
  } else if (method == "registerApplication"){
    args.assertArrayFmt("s");
    registerApplication(args,ret);
  } else if(method == "_list"){ 
    ret.push(AmArg("postDSMEvent"));
    ret.push(AmArg("reloadDSMs"));
    ret.push(AmArg("loadDSM"));
    ret.push(AmArg("loadDSMWithPaths"));
    ret.push(AmArg("preloadModules"));
    ret.push(AmArg("preloadModule"));
    ret.push(AmArg("hasDSM"));
    ret.push(AmArg("listDSMs"));
    ret.push(AmArg("registerApplication"));
  }  else
    throw AmDynInvoke::NotImplemented(method);
}
