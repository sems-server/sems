/*
 * Copyright (C) 2007 iptego GmbH
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

#include "Voicebox.h"
#include "AmUtils.h"
#include "log.h"
#include "AmPlugIn.h"
#include "AmSessionContainer.h"
#include "AmUriParser.h"
#include "../msg_storage/MsgStorageAPI.h"

#include "VoiceboxDialog.h"

#include <stdlib.h>

#include <string>
#include <vector>
using std::string;
using std::vector;

#define APP_NAME "voicebox"

EXPORT_SESSION_FACTORY(VoiceboxFactory,APP_NAME);

VoiceboxFactory::VoiceboxFactory(const string& _app_name)
  : AmSessionFactory(_app_name)
{
}

AmDynInvokeFactory* VoiceboxFactory::MessageStorage=0;

// key config
unsigned int VoiceboxFactory::repeat_key = 1;
unsigned int VoiceboxFactory::save_key = 2;
unsigned int VoiceboxFactory::delete_key = 3;
unsigned int VoiceboxFactory::startover_key = 4;
bool   VoiceboxFactory::SimpleMode=false;
string VoiceboxFactory::default_language = "";

AmPromptCollection* VoiceboxFactory::getPrompts(const string& domain, 
						const string& language,
						PromptOptions& po) {
  map<string, map<string, AmPromptCollection*> >::iterator d_it = 
    prompts.find(domain);
  if (d_it != prompts.end()) {
    map<string, AmPromptCollection*>::iterator l_it = d_it->second.find(language);
    if (l_it != d_it->second.end()) {

      // get the options to the dom/lang combination
      po = PromptOptions(false, false);
      map<string, map<string, PromptOptions> >::iterator d_oit = 
	prompt_options.find(domain);
      if (d_oit != prompt_options.end()) {
	map<string, PromptOptions>::iterator l_oit = d_oit->second.find(language);
	if (l_oit != d_oit->second.end())
	  po = l_oit->second;
      }
      
      return l_it->second;
    }
  }
  return NULL;
}

AmPromptCollection* VoiceboxFactory::findPrompts(const string& domain, 
						 const string& language,
						 PromptOptions& po) {
  AmPromptCollection* res = getPrompts(domain, language, po);
  if (res) return res;

  // best hit:
  if ((res = getPrompts(domain, default_language, po))!=NULL) return res;
  if ((res = getPrompts(domain, "",               po))!=NULL) return res;

  if ((res = getPrompts("",     language,         po))!=NULL) return res;
  if ((res = getPrompts("",     default_language, po))!=NULL) return res;
  return     getPrompts("",     "",               po);  
}

AmPromptCollection* VoiceboxFactory::loadPrompts(string prompt_base_path, 
						 string domain, string language,
						 bool load_digits) {

  AmPromptCollection* pc = new AmPromptCollection();

  string prompt_path = prompt_base_path + "/" + domain + "/" + language + "/";

  #define ADD_DEF_PROMPT(str) \
  if (pc->setPrompt(str, prompt_path + str + ".wav", APP_NAME) < 0) { \
    delete pc; \
    return NULL; \
  } 

  //		Parts for the welcome text
  ADD_DEF_PROMPT("pin_prompt");
  ADD_DEF_PROMPT("you_have");
  ADD_DEF_PROMPT("new_msgs");
  ADD_DEF_PROMPT("saved_msgs");
  ADD_DEF_PROMPT("no_msg");
  ADD_DEF_PROMPT("in_your_voicebox");
  ADD_DEF_PROMPT("and");
  //		Menu played after each message
  ADD_DEF_PROMPT("msg_menu");
  //		Menu played after last message
  ADD_DEF_PROMPT("msg_end_menu");

  //		Status acknowledgement
  ADD_DEF_PROMPT("msg_deleted");
  ADD_DEF_PROMPT("msg_saved");

  ADD_DEF_PROMPT("first_new_msg");
  ADD_DEF_PROMPT("next_new_msg");

  ADD_DEF_PROMPT("first_saved_msg");
  ADD_DEF_PROMPT("next_saved_msg");

  ADD_DEF_PROMPT("no_more_msg"); 
  //	# End of conversation
  ADD_DEF_PROMPT("bye");


  if (load_digits) {
    ADD_DEF_PROMPT("new_msg");
    ADD_DEF_PROMPT("saved_msg");

    // digits from 1 to 19
    for (unsigned int i=1;i<20;i++) {
      string str = int2str(i);
      if (pc->setPrompt(str, prompt_path + str + ".wav", APP_NAME) < 0) { 
	delete pc; 
	return NULL; 
      } 
    }
    // 20, 30, ...90 
    for (unsigned int i=20;i<100;i+=10) {
      string str = int2str(i);
      if (pc->setPrompt(str, prompt_path + str + ".wav", APP_NAME) < 0) { 
	delete pc; 
	return NULL; 
      } 
    }
    // x1 .. x9
    for (unsigned int i=1;i<10;i++) {
      string str = "x"+int2str(i);
      if (pc->setPrompt(str, prompt_path + str + ".wav", APP_NAME) < 0) { 
	delete pc; 
	return NULL; 
      } 
    }
  }

#undef ADD_DEF_PROMPT

  return pc;
}


int VoiceboxFactory::onLoad()
{
  AmConfigReader cfg;
  if(cfg.loadFile(AmConfig::ModConfigPath + string(APP_NAME)+ ".conf"))
    return -1;

  // get application specific global parameters
  configureModule(cfg);

  SimpleMode = cfg.getParameter("simple_mode") == "yes";

  default_language = cfg.getParameter("default_language");
  if (default_language.length()) {
    DBG("set default language '%s'\n", default_language.c_str());
  }

  vector<string> domains = explode(cfg.getParameter("domains"), ";");
  domains.push_back(""); // add default (empty) domain
  vector<string> languages = explode(cfg.getParameter("languages"), ";");
  languages.push_back("");// add default (empty) language

  string prompt_base_path = cfg.getParameter("prompt_base_path");
  if (prompt_base_path.empty()) {
    ERROR("prompt_base_path not set in configuration");
    return -1;
  }

  for (vector<string>::iterator dom = domains.begin(); 
       dom != domains.end(); dom++) {
    for (vector<string>::iterator lang = languages.begin();
	 lang != languages.end(); lang++) {

      string language = *lang;
      
      size_t lang_opt_pos = language.find('(');
      string lang_name = language.substr(0, lang_opt_pos);
      
      string lang_opt;
      bool lang_digits = false;
      bool lang_digitpos_right = true;
      if (lang_opt_pos != string::npos) 
	lang_opt = language.substr(lang_opt_pos, 
				   language.find(')',lang_opt_pos+1));
      if (lang_opt.find("digits=right") != string::npos) {
	lang_digits = true;
	lang_digitpos_right = true;
      } 
      if (lang_opt.find("digits=left") != string::npos) {
	lang_digits = true;
	lang_digitpos_right = true;
      } 
      
      AmPromptCollection* pc = loadPrompts(prompt_base_path, *dom, 
					   lang_name, lang_digits);
      if (NULL != pc) {
	prompts[*dom][lang_name]=pc;
	prompt_options[*dom][lang_name]=
	  PromptOptions(lang_digits, lang_digitpos_right);

	DBG("Enabled language <%s> for domain <%s>\n",
	    lang->empty()?"default":lang_name.c_str(), 
	    dom->empty()?"default":dom->c_str()
	    );
      }
    }
  }

  if (prompts.empty()) {
    ERROR("No menu voice messages found at '%s'.\n",
	  prompt_base_path.c_str());
    return -1;
  }

  string s_repeat_key = cfg.getParameter("repeat_key", "1");
  if (str2i(s_repeat_key, repeat_key)) {
    ERROR("repeat_key value '%s' unparseable.\n", 
	  s_repeat_key.c_str());
    return -1;
  }

  string s_save_key = cfg.getParameter("save_key", "2");
  if (str2i(s_save_key, save_key)) {
    ERROR("save_key value '%s' unparseable.\n", 
	  s_save_key.c_str());
    return -1;
  }

  string s_delete_key = cfg.getParameter("delete_key", "3");
  if (str2i(s_delete_key, delete_key)) {
    ERROR("delete_key value '%s' unparseable.\n", 
	  s_delete_key.c_str());
    return -1;
  }

  string s_startover_key = cfg.getParameter("startover_key", "4");
  if (str2i(s_startover_key, startover_key)) {
    ERROR("startover_key value '%s' unparseable.\n", 
	  s_startover_key.c_str());
    return -1;
  }
  
  MessageStorage = NULL;
  MessageStorage = AmPlugIn::instance()->getFactory4Di("msg_storage");
  if(NULL == MessageStorage){
    ERROR("could not load msg_storage. Load a msg_storage implementation module.\n");
    return -1;
  }

  return 0;
}

// incoming calls 
AmSession* VoiceboxFactory::onInvite(const AmSipRequest& req, const string& app_name,
				     const map<string,string>& app_params)
{
  string user;
  string pin;
  string domain;
  string language;
  string uid;
  string did;

  if(SimpleMode){
    AmUriParser p;
    p.uri = req.from_uri;
    if (!p.parse_uri()) {
      DBG("parsing From-URI '%s' failed\n", p.uri.c_str());
      throw AmSession::Exception(500, APP_NAME ": could not parse From-URI");
    }
    user = p.uri_user;
    //domain = p.uri_domain;
    domain = "default";
  }
  else {
    string iptel_app_param = getHeader(req.hdrs, PARAM_HDR, true);
  
    if (!iptel_app_param.length()) {
      throw AmSession::Exception(500, APP_NAME ": parameters not found");
    }
  

    // consistently with voicemail application:
    //  uid overrides user
    user = get_header_keyvalue(iptel_app_param, "uid", "UserID");
    if (user.empty())
      user = get_header_keyvalue(iptel_app_param, "usr", "User");


    //  did overrides domain
    domain = get_header_keyvalue(iptel_app_param, "did", "DomainID");
    if (domain.empty())
      domain = get_header_keyvalue(iptel_app_param, "dom", "Domain");


    pin = get_header_keyvalue(iptel_app_param, "pin", "PIN");
    language = get_header_keyvalue(iptel_app_param,"lng", "Language");
  }
  
  // checks
  if (user.empty()) 
    throw AmSession::Exception(500, APP_NAME ": user missing");

  if (language.empty())
    language = default_language;
  
  PromptOptions po(false, false);
  AmPromptCollection* pc = findPrompts(domain, language, po);
  if (NULL == pc)  
    throw AmSession::Exception(500, APP_NAME ": no menu for domain/language");

  return new VoiceboxDialog(user, domain, pin, pc, po);
}

