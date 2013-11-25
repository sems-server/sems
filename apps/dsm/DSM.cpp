/*
 * Copyright (C) 2008 iptego GmbH
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. This program is released under
 * the GPL with the additional exemption that compiling, linking,
 * and/or using OpenSSL is allowed.
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
#include "SystemDSM.h"

#include <string>
#include <fstream>

#include <sys/types.h>
#include <dirent.h>

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
bool DSMFactory::CheckDSM;

#ifdef USE_MONITORING
bool DSMFactory::MonitoringFullCallgraph;
bool DSMFactory::MonitoringFullTransitions;

MonSelectType DSMFactory::MonSelectCaller;
MonSelectType DSMFactory::MonSelectCallee;
vector<string> DSMFactory::MonSelectFilters;

string DSMFactory::MonSelectFallback;

#endif // USE_MONITORING

DSMFactory::DSMFactory(const string& _app_name)
  : AmSessionFactory(_app_name),AmDynInvokeFactory(_app_name),
    loaded(false),
    session_timer_f(NULL)
{
  AmEventDispatcher::instance()->addEventQueue("dsm", this);

  MainScriptConfig.diags = new DSMStateDiagramCollection();
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

  for (std::set<DSMStateDiagramCollection*>::iterator it=
	 old_diags.begin(); it != old_diags.end(); it++)
    delete *it;

  delete MainScriptConfig.diags;
}

int DSMFactory::onLoad()
{
  if (loaded)
    return 0;
  loaded = true;

  if(cfg.loadFile(AmConfig::ModConfigPath + string(MOD_NAME ".conf")))
    return -1;
 
  // get application specific global parameters
  configureModule(cfg);

  DebugDSM = cfg.getParameter("debug_raw_dsm") == "yes";
  CheckDSM = cfg.getParameter("dsm_consistency_check", "yes") == "yes";
 
  if (!loadPrompts(cfg))
    return -1;

  if (!loadPromptSets(cfg))
    return -1;

  if (!loadDiags(cfg, MainScriptConfig.diags))
    return -1;

  vector<string> registered_apps;
  if (!registerApps(cfg, MainScriptConfig.diags, registered_apps))
    return -1;

  InboundStartDiag = cfg.getParameter("inbound_start_diag");
  if (InboundStartDiag.empty()) {
    INFO("no 'inbound_start_diag' set in config. "
	 "inbound calls with application 'dsm' disabled.\n");
  }

  OutboundStartDiag = cfg.getParameter("outbound_start_diag");
  if (OutboundStartDiag.empty()) {
    INFO("no 'outbound_start_diag' set in config. "
	 "outbound calls with application 'dsm' disabled.\n");
  }

  if (!InboundStartDiag.empty())
    AmPlugIn::instance()->registerFactory4App("dsm",this);

  MainScriptConfig.config_vars.insert(cfg.begin(), cfg.end());

//   for (std::map<string,string>::const_iterator it = 
// 	 cfg.begin(); it != cfg.end(); it++) 
//     MainScriptConfig.config_vars[it->first] = it->second;

  MainScriptConfig.RunInviteEvent = cfg.getParameter("run_invite_event")=="yes";

  MainScriptConfig.SetParamVariables = cfg.getParameter("set_param_variables")=="yes";

  vector<string> system_dsms = explode(cfg.getParameter("run_system_dsms"), ",");
  for (vector<string>::iterator it=system_dsms.begin(); it != system_dsms.end(); it++) {
    string status;
    if (createSystemDSM("main", *it, false /* reload */, status)) {
      DBG("created SystemDSM '%s'\n", it->c_str());
    } else {
      ERROR("creating system DSM '%s': '%s'\n", it->c_str(), status.c_str());
      return -1;
    }
  }

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

  MonSelectFilters  = explode(cfg.getParameter("monitor_select_filters"), ",");
  string filters;
  for (vector<string>::iterator it = 
	 MonSelectFilters.begin(); it != MonSelectFilters.end(); it++) {
    if (it != MonSelectFilters.begin()) 
      filters += ", ";
    filters+=*it;
  }
  if (MonSelectFilters.size()) {
    DBG("using additional monitor app select filters: %s\n", 
	filters.c_str());
  } else {
    DBG("not using additional monitor app select filters\n");
  }
  MonSelectFallback = cfg.getParameter("monitor_select_fallback");
#endif

  string conf_d_dir = cfg.getParameter("conf_dir");
  if (!conf_d_dir.empty()) {
    if (conf_d_dir[conf_d_dir.length()-1] != '/')
      conf_d_dir += '/';

    DBG("processing configurations in '%s'...\n", conf_d_dir.c_str());
    int err=0;
    struct dirent* entry;
    DIR* dir = opendir(conf_d_dir.c_str());
    
    if(!dir){
      INFO("DSM config files loader (%s): %s\n",
	    conf_d_dir.c_str(), strerror(errno));
    } else {
      while( ((entry = readdir(dir)) != NULL) && (err == 0) ){
	string conf_name = string(entry->d_name);
	
	if (conf_name.find(".conf",conf_name.length()-5) == string::npos){
	  continue;
	}
        
	string conf_file_name = conf_d_dir + conf_name;
	
	DBG("loading %s ...\n",conf_file_name.c_str());
	if (!loadConfig(conf_file_name, conf_name, false, NULL)) 
	  return -1;

      }
      closedir(dir);
    }
  }

  if(cfg.hasParameter("enable_session_timer") &&
     (cfg.getParameter("enable_session_timer") == string("yes")) ){
    DBG("enabling session timers\n");
    session_timer_f = AmPlugIn::instance()->getFactory4Seh("session_timer");
    if(session_timer_f == NULL){
      ERROR("Could not load the session_timer module: disabling session timers.\n");
    }
  }

  return 0;
}

bool DSMFactory::loadPrompts(AmConfigReader& cfg) {

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
    return false;

  return true;
}

bool DSMFactory::loadPromptSets(AmConfigReader& cfg) {
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
  return true;
}

bool DSMFactory::loadDiags(AmConfigReader& cfg, DSMStateDiagramCollection* m_diags) {
  string DiagPath = cfg.getParameter("diag_path");
  if (DiagPath.length() && DiagPath[DiagPath.length()-1] != '/')
    DiagPath += '/';

  string ModPath = cfg.getParameter("mod_path");

  string err;
  int res = preloadModules(cfg, err, ModPath);
  if (res<0) {
    ERROR("%s\n", err.c_str());
    return false;
  }

  // todo: pass preloaded mods to chart reader

  string LoadDiags = cfg.getParameter("load_diags");
  vector<string> diags_names = explode(LoadDiags, ",");
  for (vector<string>::iterator it=
	 diags_names.begin(); it != diags_names.end(); it++) {
    if (!m_diags->loadFile(DiagPath+*it+".dsm", *it, DiagPath, ModPath, DebugDSM, CheckDSM)) {
      ERROR("loading %s from %s\n", 
	    it->c_str(), (DiagPath+*it+".dsm").c_str());
      return false;
    }
  }

  return true;
}

bool DSMFactory::registerApps(AmConfigReader& cfg, DSMStateDiagramCollection* m_diags,
			      vector<string>& register_names) {
  string RegisterDiags = cfg.getParameter("register_apps");
  register_names = explode(RegisterDiags, ",");
  for (vector<string>::iterator it=
	 register_names.begin(); it != register_names.end(); it++) {
    if (m_diags->hasDiagram(*it)) {
      bool res = AmPlugIn::instance()->registerFactory4App(*it,this);
      if(res)
	INFO("DSM state machine registered: %s.\n",
	     it->c_str());
    } else {
      ERROR("trying to register application '%s' which is not loaded.\n",
	    it->c_str());
      return false;
    }
  }
  return true;
}

// DI interface function
void DSMFactory::loadConfig(const AmArg& args, AmArg& ret) {
  string file_name = args.get(0).asCStr();
  string diag_name = args.get(1).asCStr();

  if (loadConfig(file_name, diag_name, true, NULL)) {
    ret.push(200);
    ret.push("OK");
  } else {
    ret.push(500);
    ret.push("reload config failed");
  }
}

bool DSMFactory::loadConfig(const string& conf_file_name, const string& conf_name, 
			    bool live_reload, DSMStateDiagramCollection* m_diags) {

  string script_name = conf_name.substr(0, conf_name.length()-5); // - .conf
  DBG("loading %s from %s ...\n", script_name.c_str(), conf_file_name.c_str());
  AmConfigReader cfg;
  if(cfg.loadFile(conf_file_name))
    return false;

  DSMScriptConfig script_config;
  script_config.RunInviteEvent = 
    cfg.getParameter("run_invite_event")=="yes";

  script_config.SetParamVariables = 
    cfg.getParameter("set_param_variables")=="yes";

  script_config.config_vars.insert(cfg.begin(), cfg.end());

  if (live_reload) {
    INFO("live DSM config reload does NOT reload prompts and prompt sets!\n");
    INFO("(see http://tracker.iptel.org/browse/SEMS-68)\n");
  } else {
    if (!loadPrompts(cfg))
      return false;

    if (!loadPromptSets(cfg))
      return false;
  }

  DSMStateDiagramCollection* used_diags;
  if (m_diags != NULL)
    used_diags = m_diags;     // got this from caller (main diags)
  else {
    // create a new set of diags 
    used_diags = script_config.diags = new DSMStateDiagramCollection();
  }

  if (!loadDiags(cfg, used_diags))
    return false;

  vector<string> registered_apps;
  if (!registerApps(cfg, used_diags, registered_apps))
    return false;

  ScriptConfigs_mut.lock();
  try {
    Name2ScriptConfig[script_name] = script_config;
    // set ScriptConfig to this for all registered apps' names
    for (vector<string>::iterator reg_app_it=
	   registered_apps.begin(); reg_app_it != registered_apps.end(); reg_app_it++) {
      string& app_name = *reg_app_it;
      // dispose of the old one, if it exists
      map<string, DSMScriptConfig>::iterator it=ScriptConfigs.find(app_name);
      if (it != ScriptConfigs.end()) {
	// may be in use by active call - don't delete but save to 
	// old_diags for garbage collection (destructor)
	if (it->second.diags != NULL)
	  old_diags.insert(it->second.diags);   
      }
      
      // overwrite with new config
      ScriptConfigs[app_name] = script_config;
    }
  } catch(...) {
    ScriptConfigs_mut.unlock();
    throw;
  }
  ScriptConfigs_mut.unlock();

  bool res = true;

  vector<string> system_dsms = explode(cfg.getParameter("run_system_dsms"), ",");
  for (vector<string>::iterator it=system_dsms.begin(); it != system_dsms.end(); it++) {
    string status;
    if (createSystemDSM(script_name, *it, live_reload, status)) {
    } else {
      ERROR("creating system DSM '%s': '%s'\n", it->c_str(), status.c_str());
      res = false;
    }
  }

  return res;
}


void DSMFactory::prepareSession(DSMCall* s) {
  s->setPromptSets(prompt_sets);
  setupSessionTimer(s);
}

void DSMFactory::setupSessionTimer(AmSession* s) {
  if (NULL != session_timer_f) {

    AmSessionEventHandler* h = session_timer_f->getHandler(s);
    if (NULL == h)
      return;

    if(h->configure(cfg)){
      ERROR("Could not configure the session timer: disabling session timers.\n");
      delete h;
    } else {
      s->addHandler(h);
    }
  }
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
  vector<string> items = explode(getHeader(hdrs, PARAM_HDR, true), ";");
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
      vars[it->first+"_size"] = int2str((unsigned int)it->second.size());
      for (size_t i=0;i<it->second.size();i++) {
	if (it->second.get(i).getType() == AmArg::CStr)
	  vars[it->first+"_"+int2str((unsigned int)i)] = it->second.get(i).asCStr();
	else
	  vars[it->first+"_"+int2str((unsigned int)i)] = AmArg::print(it->second.get(i));
      }
    } else {
      vars[it->first] = AmArg::print(it->second);	
    }
  }
}

void DSMFactory::runMonitorAppSelect(const AmSipRequest& req, string& start_diag, 
				     map<string, string>& vars) {
#define FALLBACK_OR_EXCEPTION(code, reason)				\
  if (MonSelectFallback.empty()) {					\
    throw AmSession::Exception(code, reason);				\
  } else {								\
    DBG("falling back to '%s'\n", MonSelectFallback.c_str());		\
    start_diag = MonSelectFallback;					\
    return;								\
  }

#ifdef USE_MONITORING
      if (NULL == MONITORING_GLOBAL_INTERFACE) {
	ERROR("using $(mon_select) but monitoring not loaded\n");
	FALLBACK_OR_EXCEPTION(500, "Internal Server Error");
      }

      AmArg di_args, ret;
      if (MonSelectCaller != MonSelect_NONE) {
	AmUriParser from_parser;
	if (MonSelectCaller == MonSelect_FROM)
	  from_parser.uri = req.from_uri;
	else {
	  size_t end;
	  string pai = getHeader(req.hdrs, SIP_HDR_P_ASSERTED_IDENTITY, true);
	  if (!from_parser.parse_contact(pai, 0, end)) {
	    WARN("Failed to parse " SIP_HDR_P_ASSERTED_IDENTITY " '%s'\n",
		  pai.c_str());
	    FALLBACK_OR_EXCEPTION(500, "Internal Server Error");
	  }
	}

	if (!from_parser.parse_uri()) {
	  DBG("failed to parse caller uri '%s'\n", from_parser.uri.c_str());
	  FALLBACK_OR_EXCEPTION(500, "Internal Server Error");
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
	    FALLBACK_OR_EXCEPTION(500, "Internal Server Error");
	  }
	  if (!to_parser.parse_uri()) {
	    DBG("failed to parse callee uri '%s'\n", to_parser.uri.c_str());
	    FALLBACK_OR_EXCEPTION(500, "Internal Server Error");
	  }
	  callee_filter.push(to_parser.uri_user);
	}
	  
	DBG(" && looking for callee=='%s'\n", req.user.c_str());
	di_args.push(callee_filter);
      }
      // apply additional filters
      if (MonSelectFilters.size()) {
	string app_params = getHeader(req.hdrs, PARAM_HDR);
	for (vector<string>::iterator it = 
	       MonSelectFilters.begin(); it != MonSelectFilters.end(); it++) {
	  AmArg filter;
	  filter.push(*it); // avp name
	  string app_param_val = get_header_keyvalue(app_params, *it);
	  filter.push(app_param_val);
	  di_args.push(filter);
	  DBG(" && looking for %s=='%s'\n", it->c_str(), app_param_val.c_str());
	}
      }

      MONITORING_GLOBAL_INTERFACE->invoke("listByFilter",di_args,ret);
      
      if ((ret.getType()!=AmArg::Array)||
	  !ret.size()) {
	DBG("call info not found. caller uri %s, r-uri %s\n", 
	     req.from_uri.c_str(), req.r_uri.c_str());
	FALLBACK_OR_EXCEPTION(500, "Internal Server Error");
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
	FALLBACK_OR_EXCEPTION(500, "Internal Server Error");
      }

      AmArg& sess_dict = sess_params.get(0);
      if (sess_dict.hasMember("app")) {
	start_diag = sess_dict["app"].asCStr();
	DBG("selected application '%s' for session\n", start_diag.c_str());
      } else {
	ERROR("selected session params don't contain 'app'\n");
	FALLBACK_OR_EXCEPTION(500, "Internal Server Error");
      }
      AmArg2DSMStrMap(sess_dict["appParams"], vars);
      vars["mon_session_record"] = session_id;
	
#else
      ERROR("using $(mon_select) for dsm application, "
	    "but compiled without monitoring support!\n");
      throw AmSession::Exception(500, "Internal Server Error");
#endif

#undef FALLBACK_OR_EXCEPTION
}
 
AmSession* DSMFactory::onInvite(const AmSipRequest& req, const string& app_name,
				const map<string,string>& app_params)
{
  string start_diag;
  map<string, string> vars;

  if (app_name == MOD_NAME) {
    if (InboundStartDiag.empty()) {
      ERROR("no inbound calls allowed\n");
      throw AmSession::Exception(488, "Not Acceptable Here");
    }
    if (InboundStartDiag=="$(mon_select)") {
      runMonitorAppSelect(req, start_diag, vars);
    } else {
      start_diag = InboundStartDiag;
    }
  } else {
    start_diag = app_name;
  }

  DBG("start_diag = %s\n",start_diag.c_str());

  // determine run configuration for script
  DSMScriptConfig call_config;
  ScriptConfigs_mut.lock();
  map<string, DSMScriptConfig>::iterator sc=ScriptConfigs.find(start_diag);
  if (sc == ScriptConfigs.end()) 
    call_config = MainScriptConfig;
  else 
    call_config = sc->second;

  DSMCall* s = new DSMCall(call_config, &prompts, *call_config.diags, start_diag, NULL);

  ScriptConfigs_mut.unlock();

  prepareSession(s);
  addVariables(s, "config.", call_config.config_vars);

  if (call_config.SetParamVariables) 
    addParams(s, req.hdrs);

  if (!vars.empty())
    addVariables(s, "", vars);

  return s;
}

// outgoing call
AmSession* DSMFactory::onInvite(const AmSipRequest& req, const string& app_name,
				AmArg& session_params) 
{

  string start_diag;

  if (app_name == MOD_NAME) {
    if (OutboundStartDiag.empty()) {
      ERROR("no outbound calls allowed\n");
      throw AmSession::Exception(488, "Not Acceptable Here");
    }
  } else {
    start_diag = app_name;
  }

  UACAuthCred* cred = NULL;
  map<string, string> vars;
  // Creds
  if (session_params.getType() == AmArg::AObject) {
    AmObject* cred_obj = session_params.asObject();
    if (cred_obj)
      cred = dynamic_cast<UACAuthCred*>(cred_obj);
  } else if (session_params.getType() == AmArg::Array) {
    DBG("session params is array - size %zd\n", session_params.size());
    // Creds
    cred = AmUACAuth::unpackCredentials(session_params.get(0));
    // Creds + vars
    if (session_params.size()>1 && 
	session_params.get(1).getType() == AmArg::Struct) {
      AmArg2DSMStrMap(session_params.get(1), vars);
    }
  } else if (session_params.getType() == AmArg::Struct) {
    // vars
    AmArg2DSMStrMap(session_params, vars);
  }

  DSMScriptConfig call_config;
  ScriptConfigs_mut.lock();
  map<string, DSMScriptConfig>::iterator sc=ScriptConfigs.find(start_diag);
  if (sc == ScriptConfigs.end())
    call_config = MainScriptConfig;
  else 
    call_config = sc->second;

  DSMCall* s = new DSMCall(call_config, &prompts, *call_config.diags, start_diag, cred); 

  ScriptConfigs_mut.unlock();

  prepareSession(s);  

  addVariables(s, "config.", call_config.config_vars);
  if (!vars.empty())
    addVariables(s, "", vars);

  if (call_config.SetParamVariables) 
    addParams(s, req.hdrs); 

  if (NULL == cred) {
    DBG("outgoing DSM call will not be authenticated.\n");
  } else {
    AmUACAuth::enable(s);
  }

  return s;
}

bool DSMFactory::createSystemDSM(const string& config_name, const string& start_diag, bool reload, string& status) {
  bool res = true;

  DSMScriptConfig* script_config = NULL;
  ScriptConfigs_mut.lock();
  if (config_name == "main")
    script_config = &MainScriptConfig;
  else {
    map<string, DSMScriptConfig>::iterator it = Name2ScriptConfig.find(config_name);
    if (it != Name2ScriptConfig.end()) 
      script_config = &it->second;
  }
  if (script_config==NULL) {
    status = "Error: Script config '"+config_name+"' not found, in [";
    for (map<string, DSMScriptConfig>::iterator it = 
	   Name2ScriptConfig.begin(); it != Name2ScriptConfig.end(); it++) {
      if (it != Name2ScriptConfig.begin()) 
	status+=", ";
      status += it->first; 
    }
    status += "]";
    res = false;
  } else {
    SystemDSM* s = new SystemDSM(*script_config, start_diag, reload);
    s->start();
    // add to garbage collector
    AmThreadWatcher::instance()->add(s);
    status = "OK";
  }
  ScriptConfigs_mut.unlock();
  return res;
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
    if (!new_diags->loadFile(DiagPath+*it+".dsm", *it, DiagPath, ModPath,
			     DebugDSM, CheckDSM)) {
      ERROR("loading %s from %s\n", 
	    it->c_str(), (DiagPath+*it+".dsm").c_str());
      ret.push(500);
      ret.push("loading " +*it+ " from "+ DiagPath+*it+".dsm");
      return;
    }
  }
  ScriptConfigs_mut.lock();
  old_diags.insert(MainScriptConfig.diags);
  MainScriptConfig.diags = new_diags; 
  ScriptConfigs_mut.unlock();

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
  vector<string> names;
  ScriptConfigs_mut.lock();

  try {
    if (isArgUndef(args) || !args.size())
      names = MainScriptConfig.diags->getDiagramNames();
    else {
      if (isArgCStr(args.get(0))) {
	map<string, DSMScriptConfig>::iterator i=
	  ScriptConfigs.find(args.get(0).asCStr());
	if (i!= ScriptConfigs.end()) 
	  names = i->second.diags->getDiagramNames();
      }
    }
  } catch (...) {
    ScriptConfigs_mut.unlock();
    throw;
  }

  ScriptConfigs_mut.unlock();

  for (vector<string>::iterator it=
	 names.begin(); it != names.end(); it++) {
    ret.push(*it);
  }
}

bool DSMFactory::hasDSM(const string& dsm_name, const string& conf_name) {
  bool res = false; 
  if (conf_name.empty())
    res = MainScriptConfig.diags->hasDiagram(dsm_name);
  else {
      map<string, DSMScriptConfig>::iterator i=
	ScriptConfigs.find(conf_name);
	if (i!= ScriptConfigs.end()) 
	  res = i->second.diags->hasDiagram(dsm_name);
  }
  return res;
}

void DSMFactory::hasDSM(const AmArg& args, AmArg& ret) {
  string conf_name;
  if (args.size()>1 && isArgCStr(args.get(1)))
    conf_name = args.get(1).asCStr();

  bool res;

  ScriptConfigs_mut.lock();
  try {
    res = hasDSM(args.get(0).asCStr(), conf_name); 
  } catch(...) {
    ScriptConfigs_mut.unlock();
    throw;
  }
  ScriptConfigs_mut.unlock();

  if (res)
    ret.push("1");
  else
    ret.push("0");
}

void DSMFactory::registerApplication(const AmArg& args, AmArg& ret) {
  string diag_name = args.get(0).asCStr();
  string conf_name;
  if (args.size()>1 && isArgCStr(args.get(1)))
    conf_name = args.get(1).asCStr();
  bool has_diag;

  ScriptConfigs_mut.lock();
  try {
    has_diag = hasDSM(diag_name, conf_name); 
  } catch(...) {
    ScriptConfigs_mut.unlock();
    throw;
  }
  ScriptConfigs_mut.unlock();  

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

  ScriptConfigs_mut.lock();
  try {
    if (MainScriptConfig.diags->hasDiagram(dsm_name)) {
      ret.push(400);
      ret.push("DSM named '" + dsm_name + "' already loaded (use reloadDSMs to reload all)");
    } else {
      if (!MainScriptConfig.diags->loadFile(dsm_file_name, dsm_name, DiagPath, ModPath, DebugDSM, CheckDSM)) {
	ret.push(500);
	ret.push("error loading "+dsm_name+" from "+ dsm_file_name);
      } else {
	ret.push(200);
	ret.push("loaded "+dsm_name+" from "+ dsm_file_name);
      }
    }
  } catch(...) {
    ScriptConfigs_mut.unlock();
    throw;
  }
  ScriptConfigs_mut.unlock();
}

void DSMFactory::loadDSMWithPaths(const AmArg& args, AmArg& ret) {
  string dsm_name  = args.get(0).asCStr();
  string diag_path = args.get(1).asCStr();
  string mod_path  = args.get(2).asCStr();

  string res = "OK";
  ScriptConfigs_mut.lock();
  try {
    if (MainScriptConfig.diags->hasDiagram(dsm_name)) {
      ret.push(400);
      ret.push("DSM named '" + dsm_name + "' already loaded (use reloadDSMs to reload all)");
    } else {
      if (!MainScriptConfig.diags->loadFile(diag_path+dsm_name+".dsm", dsm_name, diag_path, mod_path, DebugDSM, CheckDSM)) {
	ret.push(500);
	ret.push("error loading "+dsm_name+" from "+ diag_path+dsm_name+".dsm");
      } else {
	ret.push(200);
	ret.push("loaded "+dsm_name+" from "+ diag_path+dsm_name+".dsm");
      }
    }
  } catch(...) {
    ScriptConfigs_mut.unlock();
    throw;
  }
  ScriptConfigs_mut.unlock();
}

bool DSMFactory::addScriptDiagsToEngine(const string& config_set,
					DSMStateEngine* engine,
					map<string,string>& config_vars,
					bool& SetParamVariables) {
  bool res = false;
  ScriptConfigs_mut.lock();
  try {
    map<string, DSMScriptConfig>::iterator it=Name2ScriptConfig.find(config_set);
    if (it!=Name2ScriptConfig.end()) {
      res = true;
      it->second.diags->addToEngine(engine);
      config_vars = it->second.config_vars;
      SetParamVariables = it->second.SetParamVariables;
    }
  } catch(...) {
    ScriptConfigs_mut.unlock();
    throw;
  }
  ScriptConfigs_mut.unlock();
  return res;
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
  } else if (method == "loadConfig"){
    args.assertArrayFmt("ss");
    loadConfig(args,ret);
  } else if (method == "createSystemDSM"){
    args.assertArrayFmt("ss");
    string status;
    if (createSystemDSM(args.get(0).asCStr(), args.get(1).asCStr(), false, status)) {
      ret.push(200);
      ret.push(status);
    } else {
      ret.push(500);
      ret.push(status);
    }
  } else if(method == "_list"){ 
    ret.push(AmArg("postDSMEvent"));
    ret.push(AmArg("reloadDSMs"));
    ret.push(AmArg("loadDSM"));
    ret.push(AmArg("loadDSMWithPaths"));
    ret.push(AmArg("preloadModules"));
    ret.push(AmArg("preloadModule"));
    ret.push(AmArg("loadConfig"));
    ret.push(AmArg("hasDSM"));
    ret.push(AmArg("listDSMs"));
    ret.push(AmArg("registerApplication"));
    ret.push(AmArg("createSystemDSM"));
  }  else
    throw AmDynInvoke::NotImplemented(method);
}

