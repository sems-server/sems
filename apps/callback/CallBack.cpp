/*
 * $Id: CallBack.cpp 288 2007-03-28 16:32:02Z sayer $
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

#include "CallBack.h"
#include "AmUtils.h"
#include "log.h"
#include "AmUAC.h"
#include "AmPlugIn.h"

#include <stdlib.h>

EXPORT_SESSION_FACTORY(CallBackFactory,MOD_NAME);
string CallBackFactory::gw_user;
string CallBackFactory::gw_domain;
string CallBackFactory::auth_user;
string CallBackFactory::auth_pwd;

CallBackFactory::CallBackFactory(const string& _app_name)
  : AmSessionFactory(_app_name),
    configured(false)
{
}

PlayoutType CallBackFactory::m_PlayoutType = ADAPTIVE_PLAYOUT;

int CallBackFactory::onLoad()
{
  AmConfigReader cfg;
  if(cfg.loadFile(AmConfig::ModConfigPath + string(MOD_NAME)+ ".conf"))
    return -1;

  // get application specific global parameters
  configureModule(cfg);

  // get prompts
  AM_PROMPT_START;
  AM_PROMPT_ADD(WELCOME_PROMPT,  WELCOME_PROMPT ".wav");
  AM_PROMPT_END(prompts, cfg, MOD_NAME);

  string DigitsDir = cfg.getParameter("digits_dir");
  if (DigitsDir.length() && DigitsDir[DigitsDir.length()-1]!='/')
    DigitsDir+='/';

  if (!DigitsDir.length()) {
    ERROR("No digits_dir specified in configuration.\n");
  }
  for (int i=0;i<10;i++) 
    prompts.setPrompt(int2str(i), DigitsDir+int2str(i)+".wav", MOD_NAME);

  string playout_type = cfg.getParameter("playout_type");
  if (playout_type == "simple") {
    m_PlayoutType = SIMPLE_PLAYOUT;
    DBG("Using simple (fifo) buffer as playout technique.\n");
  } else 	if (playout_type == "adaptive_jb") {
    m_PlayoutType = JB_PLAYOUT;
    DBG("Using adaptive jitter buffer as playout technique.\n");
  } else {
    DBG("Using adaptive playout buffer as playout technique.\n");
  }
  
  string accept_caller_re_str = cfg.getParameter(ACCEPT_CALLER_RE);
  if (!accept_caller_re_str.length()) {
    ERROR("no '" ACCEPT_CALLER_RE "' set.\n");
    return -1;
  } else {
    if (regcomp(&accept_caller_re, accept_caller_re_str.c_str(), 
		 REG_EXTENDED|REG_NOSUB)) {
      ERROR("unable to compile caller RE '%s'.\n",
	    accept_caller_re_str.c_str());
      return -1;
    }
  }

  gw_user = cfg.getParameter("gw_user");
  if (!gw_user.length()) {
    ERROR("need gw_user configured!\n");
    return -1;
  }

  gw_domain = cfg.getParameter("gw_domain");
  if (!gw_domain.length()) {
    ERROR("need gw_domain configured!\n");
    return -1;
  }

  auth_user = cfg.getParameter("auth_user");
  if (!auth_user.length())
    auth_user = gw_user; // default to user

  auth_pwd = cfg.getParameter("auth_pwd");
  if (!auth_pwd.length()) {
    ERROR("need auth_pwd configured!\n");
    return -1;
  }

  cb_wait = cfg.getParameterInt("cb_wait", 5);
  DBG("cb_wait set to %d\n", cb_wait);

  DBG("starting callback thread. (%ld)\n", (long)this);
  start();

  return 0;
}

// incoming calls 
AmSession* CallBackFactory::onInvite(const AmSipRequest& req)
{
  // or req.from -> with display name ? 
  DBG("received INVITE from '%s'\n", req.from_uri.c_str());
  if (!regexec(&accept_caller_re, req.from_uri.c_str(), 0,0,0)) {
    DBG("accept_caller_re matched.\n");
    time_t now;
    time(&now);
    // q&d
    string from_user = req.from_uri.substr(req.from_uri.find("sip:")+4);
    from_user = from_user.substr(0, from_user.find("@")); 
    DBG("INVITE user '%s'\n", from_user.c_str());
    if (from_user.length()) {
      scheduled_calls_mut.lock();
      scheduled_calls.insert(make_pair(now + cb_wait, from_user));
      scheduled_calls_mut.unlock();
    }
    
    DBG("inserted into callback thread. (%ld)\n", (long)this);
    // or some other reason
    throw AmSession::Exception(486, "Busy here (call you back l8r)"); 
  } else {
    DBG("accept_caller_re not matched.\n");
    // or something else
    throw AmSession::Exception(603, "Decline"); 
  }
  
  return 0;
}

// outgoing calls 
AmSession* CallBackFactory::onInvite(const AmSipRequest& req,
				     AmArg& session_params)
{
  UACAuthCred* cred = NULL;
  if (session_params.getType() == AmArg::AObject) {
    ArgObject* cred_obj = session_params.asObject();
    if (cred_obj)
      cred = dynamic_cast<UACAuthCred*>(cred_obj);
  }

  AmSession* s = new CallBackDialog(prompts, cred); 
  addAuthHandler(s);
  return s;
}

// this could have been made easier with a user_timer. 
void CallBackFactory::run() {
  DBG("running CallBack thread.\n");
  while (true) {
    scheduled_calls_mut.lock();
    vector<string> todo;
    time_t now;
    time(&now);
    multimap<time_t, string>::iterator it = 
      scheduled_calls.begin();
    while (it != scheduled_calls.end()) {
      if (it->first > now)
	break;
      todo.push_back(it->second);
      scheduled_calls.erase(it);
      it = scheduled_calls.begin(); 
    }      
    scheduled_calls_mut.unlock();

    for (vector<string>::iterator it=todo.begin(); 
	 it != todo.end(); it++)  {
      createCall(*it);
    }

    sleep(1);
  }
}

void CallBackFactory::on_stop() {
}

void CallBackFactory::createCall(const string& number) {
  AmArg* a = new AmArg();
  a->setBorrowedPointer(new UACAuthCred("", auth_user, auth_pwd));
  
  string luser = "cb";
  string to = "sip:"+ number + "@" + gw_domain;
  string from = "sip:"+ gw_user + "@" + gw_domain;

  AmUAC::dialout(luser, 
		 MOD_NAME,  
		 to,  
		 "<" + from +  ">", from, 
		 "<" + to + ">", 
		 string(""), // local tag
		 string("X-Extra: fancy\r\n"), // hdrs
		 a);
}

CallBackDialog::CallBackDialog(AmPromptCollection& prompts,
			       UACAuthCred* cred)
  : play_list(this),  prompts(prompts), cred(cred),
    state(CBNone)
{
  // set configured playout type
  rtp_str.setPlayoutType(CallBackFactory::m_PlayoutType);
}

CallBackDialog::~CallBackDialog()
{
  prompts.cleanup((long)this);
  play_list.close(false);
}


void CallBackDialog::onSessionStart(const AmSipRequest& req) { 
  ERROR("incoming calls not supported!\n");
  setStopped();
  dlg.bye();
}

void CallBackDialog::onSessionStart(const AmSipReply& rep) { 
  state = CBEnteringNumber;    
  prompts.addToPlaylist(WELCOME_PROMPT,  (long)this, play_list);
  // set the playlist as input and output
  setInOut(&play_list,&play_list);
}
 
void CallBackDialog::onDtmf(int event, int duration)
{
  DBG("CallBackDialog::onDtmf: event %d duration %d\n", 
      event, duration);

  if (CBEnteringNumber == state) {
    // not yet in conference
    if (event<10) {
      call_number += int2str(event);
      DBG("added '%s': number is now '%s'.\n", 
	  int2str(event).c_str(), call_number.c_str());
    } else if (event==10 || event==11) {
      // pound and star key
      // if required add checking of pin here...
      if (!call_number.length()) {
	prompts.addToPlaylist(WELCOME_PROMPT,  (long)this, play_list);
      } else {
	state = CBTellingNumber;
	play_list.close();
	for (size_t i=0;i<call_number.length();i++) {
	  string num = "";
	  num[0] = call_number[i]; // this works? 
	  DBG("adding '%s' to playlist.\n", num.c_str());
	  prompts.addToPlaylist(num,
				(long)this, play_list);
	}
      }
    }
  }
}

void CallBackDialog::process(AmEvent* ev)
{
  // audio events
  AmAudioEvent* audio_ev = dynamic_cast<AmAudioEvent*>(ev);
  if (audio_ev  && 
      audio_ev->event_id == AmAudioEvent::noAudio) {
    DBG("########## noAudio event #########\n");
    if (CBTellingNumber == state) {
      state = CBConnecting;
      string callee = "sip:" + call_number + "@" + CallBackFactory::gw_domain;
      string caller = "sip:" + CallBackFactory::gw_user + "@" +  CallBackFactory::gw_domain;
      connectCallee(callee, callee, 
		    caller, caller);
    }
    return;
  }
  
  AmB2ABSession::process(ev);
}

// need this to pass credentials...
AmB2ABCalleeSession* CallBackDialog::createCalleeSession() {
  CallBackCalleeDialog* sess = new CallBackCalleeDialog(getLocalTag(), cred);
  addAuthHandler(sess);
  return sess;
}

CallBackCalleeDialog::CallBackCalleeDialog(const string& other_tag, 
				     UACAuthCred* cred) 
  : AmB2ABCalleeSession(other_tag), cred(cred)
{
  setDtmfDetectionEnabled(false);
}

CallBackCalleeDialog::~CallBackCalleeDialog() {
}


void addAuthHandler(AmSession* s) {
  AmSessionEventHandlerFactory* uac_auth_f = 
    AmPlugIn::instance()->getFactory4Seh("uac_auth");
  if (uac_auth_f != NULL) {
    DBG("UAC Auth enabled for new session.\n");
    AmSessionEventHandler* h = uac_auth_f->getHandler(s);
    if (h != NULL )
      s->addHandler(h);
  } else {
    ERROR("uac_auth interface not accessible. "
	  "Load uac_auth for authenticated calls.\n");
  }		
}
