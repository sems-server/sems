/*
 * Copyright (C) 2008 iptego GmbH
 * Based on the concept of auth_b2b, Copyright (C) 2008 iptego GmbH
 * Based on the concept of mycc, Copyright (C) 2007 Sipwise GmbH
 * Based on the concept of sw_prepaid_sip, Copyright (C) 2002-2003 Fhg Fokus
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

#include "CallTimer.h"

#include "log.h"
#include "AmUtils.h"
#include "AmAudio.h"
#include "AmPlugIn.h"
#include "AmMediaProcessor.h"
#include "AmConfigReader.h"
#include "AmSessionContainer.h"

#define TIMERID_CALL_TIMER 1

#define DEFAULT_CALL_TIMER 1200  // 2 h default call timer

unsigned int CallTimerFactory::DefaultCallTimer; 
bool         CallTimerFactory::UseAppParam = true;

EXPORT_SESSION_FACTORY(CallTimerFactory,MOD_NAME);

CallTimerFactory::CallTimerFactory(const string& _app_name)
: AmSessionFactory(_app_name)
{
}

CallTimerFactory::~CallTimerFactory() {
}



int CallTimerFactory::onLoad()
{
  AmConfigReader cfg;
  if(cfg.loadFile(AmConfig::ModConfigPath + string(MOD_NAME ".conf"))) {
    DBG("using default timer of %d seconds\n", DEFAULT_CALL_TIMER);
    DefaultCallTimer = DEFAULT_CALL_TIMER;
  } else {
    DefaultCallTimer  = cfg.getParameterInt("default_call_time", DEFAULT_CALL_TIMER);
    UseAppParam = (cfg.getParameter("use_app_param") == "yes");
  }

  if (!AmSession::timersSupported()) {
    ERROR("load session_timer plug-in for timers\n");
    return -1;
  }

  return 0;
}


AmSession* CallTimerFactory::onInvite(const AmSipRequest& req)
{
  DBG(" *** creating new call timer session ***\n");

  string app_param = getHeader(req.hdrs, PARAM_HDR, true);

  unsigned int call_time = CallTimerFactory::DefaultCallTimer;

  if (CallTimerFactory::UseAppParam) {
    if (!app_param.length()) {
      INFO("call_time: no call timer parameters found. "
	   "Using default value of %d seconds\n", 
	   CallTimerFactory::DefaultCallTimer);
    } else {
      string call_time_s = get_header_keyvalue(app_param,"t", "Timer");
      
      if (str2i(call_time_s, call_time)) {
	WARN("Error decoding call time value '%s'. Using default time of %d\n",
	     call_time_s.c_str(), call_time);
      } 
    }
  }

  DBG("using call timer %d seconds\n", call_time);

  return new CallTimerDialog(call_time);
}


CallTimerDialog::CallTimerDialog(unsigned int call_time)
: m_state(BB_Init),
  call_time(call_time),
  AmB2BCallerSession()

{
  set_sip_relay_only(false);
}


CallTimerDialog::~CallTimerDialog()
{
}

void CallTimerDialog::onInvite(const AmSipRequest& req)
{
  if (dlg.getStatus() == AmSipDialog::Connected) {
    DBG("not acting on re-Invite\n");
    return;
  }
    
  // this will prevent us from being added to media processor
  setInOut(NULL,NULL);

  m_state = BB_Dialing;

  if(dlg.reply(req, 100, "Trying") != 0) {
    throw AmSession::Exception(500,"Failed to reply 100");
  }

  invite_req = req;

  // remove P-App-Name, P-App-Param header
  removeHeader(invite_req.hdrs, "P-App-Param");
  removeHeader(invite_req.hdrs, "P-App-Name");

  connectCallee(invite_req.to, invite_req.r_uri, true);
}

void CallTimerDialog::process(AmEvent* ev)
{
  AmPluginEvent* plugin_event = dynamic_cast<AmPluginEvent*>(ev);
  if(plugin_event && plugin_event->name == "timer_timeout") {
    int timer_id = plugin_event->data.get(0).asInt();
    if (timer_id == TIMERID_CALL_TIMER) {
      DBG("timer timeout.\n");
      terminateOtherLeg();
      dlg.bye();
      terminateLeg();
      ev->processed = true;
      return;
    }
  }
  
  AmB2BCallerSession::process(ev);
}


bool CallTimerDialog::onOtherReply(const AmSipReply& reply)
{
  bool ret = false;

  if (m_state == BB_Dialing) {
    if (reply.code < 200) {
      DBG("Callee is trying... code %d\n", reply.code);
    }
    else if(reply.code < 300) {
      if(getCalleeStatus()  == Connected) {
        m_state = BB_Connected;
        setInOut(NULL, NULL);
	// startAccounting();
	// set the call timer
	setTimer(TIMERID_CALL_TIMER, call_time);
      }
    }
    else if(reply.code == 487 && dlg.getStatus() == AmSipDialog::Pending) {
      DBG("Stopping leg A on 487 from B with 487\n");
      dlg.reply(invite_req, 487, "Request terminated");
      setStopped();
      ret = true;
    }
    else if (reply.code >= 300 && dlg.getStatus() == AmSipDialog::Connected) {
      DBG("Callee final error in connected state with code %d\n",reply.code);
      terminateLeg();
    }
    else if (reply.code >= 300 && m_state == BB_Dialing) {
      DBG("Callee final error with code %d\n",reply.code);
      AmB2BCallerSession::onOtherReply(reply);
      // reset into non-b2b mode to get possible INVITE again
      sip_relay_only = false;
    } else {
      DBG("Callee final error with code %d\n",reply.code);
      AmB2BCallerSession::onOtherReply(reply);
    }
  }
  return ret;
}


void CallTimerDialog::onOtherBye(const AmSipRequest& req)
{
//   stopAccounting();
  AmB2BCallerSession::onOtherBye(req);
}


void CallTimerDialog::onBye(const AmSipRequest& req)
{
  if (m_state == BB_Connected) {
//     stopAccounting();
  }
  terminateOtherLeg();
  setStopped();
}


void CallTimerDialog::onCancel()
{
  if(dlg.getStatus() == AmSipDialog::Pending) {
    DBG("Wait for leg B to terminate");
  }
  else {
    DBG("Canceling leg A on CANCEL since dialog is not pending");
    dlg.reply(invite_req, 487, "Request terminated");
    setStopped();
  }
}

