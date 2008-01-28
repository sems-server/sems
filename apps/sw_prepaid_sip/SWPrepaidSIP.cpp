/*
 * $Id: $
 *
 * Copyright (C) 2007 Sipwise GmbH
 * Based on the concept of mycc, Copyright (C) 2002-2003 Fhg Fokus
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

#include "SWPrepaidSIP.h"

#include "log.h"
#include "AmUtils.h"
#include "AmAudio.h"
#include "AmPlugIn.h"
#include "AmMediaProcessor.h"
#include "AmSipDialog.h"
#include "AmConfigReader.h"

#include <sys/time.h>
#include <time.h>

#define MOD_NAME "sw_prepaid_sip"

#define TIMERID_CREDIT_TIMEOUT 1
#define ACC_PLUGIN "sw_prepaid_acc"

EXPORT_SESSION_FACTORY(SWPrepaidSIPFactory,MOD_NAME);

SWPrepaidSIPFactory::SWPrepaidSIPFactory(const string& _app_name)
: AmSessionFactory(_app_name), user_timer_fact(NULL)
{
}


int SWPrepaidSIPFactory::onLoad()
{
  AmConfigReader cfg;
  if(cfg.loadFile(AmConfig::ModConfigPath + string(MOD_NAME ".conf")))
    return -1;

  string acc_plugin = cfg.getParameter("acc_plugin", ACC_PLUGIN);

  user_timer_fact = AmPlugIn::instance()->getFactory4Di("user_timer");
  if(!user_timer_fact) {
    ERROR("could not load user_timer from session_timer plug-in\n");
    return -1;
  }

  DBG("use acc plugin '%s'", acc_plugin.c_str());
  cc_acc_fact = AmPlugIn::instance()->getFactory4Di(acc_plugin);
  if(!cc_acc_fact) {
    ERROR("could not load accounting plugin '%s', please provide a valid module name\n",
      acc_plugin.c_str());
    return -1;
  }

  return 0;
}


AmSession* SWPrepaidSIPFactory::onInvite(const AmSipRequest& req)
{
  AmDynInvoke* user_timer = user_timer_fact->getInstance();
  if(!user_timer) {
    ERROR("could not get a user timer reference\n");
    throw AmSession::Exception(500,"could not get a user timer reference");
  }

  AmDynInvoke* cc_acc = cc_acc_fact->getInstance();
  if(!cc_acc) {
    ERROR("could not get an accounting reference\n");
    throw AmSession::Exception(500,"could not get an acc reference");
  }

  return new SWPrepaidSIPDialog(cc_acc, user_timer);
}


SWPrepaidSIPDialog::SWPrepaidSIPDialog(AmDynInvoke* cc_acc, AmDynInvoke* user_timer)
: m_state(CC_Init),
m_cc_acc(cc_acc), m_user_timer(user_timer),
AmB2BCallerSession()

{
  sip_relay_only = false;
  memset(&m_acc_start, 0, sizeof(struct timeval));
}


SWPrepaidSIPDialog::~SWPrepaidSIPDialog()
{
}


void SWPrepaidSIPDialog::onInvite(const AmSipRequest& req)
{
  // TODO: do reinvites get here? if so, don't set a timer then

  // TODO: errors thrown as exception don't seem to trigger a reply?

  setReceiving(false);
  AmMediaProcessor::instance()->removeSession(this);

  m_uuid = getHeader(req.hdrs,"P-Caller-Uuid");
  if(!m_uuid.length()) {
    ERROR("Application header P-Caller-Uuid not found\n");
    throw AmSession::Exception(500, "could not get UUID parameter");
  }

  m_proxy = getHeader(req.hdrs,"P-Proxy");
  if(!m_proxy.length()) {
    ERROR("Application header P-Proxy not found\n");
    throw AmSession::Exception(500, "could not get PROXY parameter");
  }

  m_ruri = getHeader(req.hdrs,"P-R-Uri");
  if(!m_ruri.length()) {
    ERROR("Application header P-R-Uri not found\n");
    throw AmSession::Exception(500, "could not get RURI parameter");
  }

  m_dest = getHeader(req.hdrs,"P-Acc-Dest");

  if(!m_dest.length()) {
    ERROR("Application header P-Acc-Dest not found\n");
    throw AmSession::Exception(500, "could not get destination pattern parameter");
  }

  DBG("UUID '%s' and pattern '%s' prepared for prepaid processing\n", m_uuid.c_str(), m_dest.c_str());

  m_starttime = time(NULL);
  m_localreq = req;

  AmArg di_args,ret;
  di_args.push(m_uuid.c_str());
  di_args.push((int)m_starttime);
  di_args.push(m_dest.c_str());
  m_cc_acc->invoke("getCredit", di_args, ret);
  m_credit = ret.get(0).asInt();

  if(m_credit == -1) {
    ERROR("Failed to fetch credit from accounting module\n");
    if(dlg.reply(req, 500, "Failed to fetch credit") != 0) {
      throw AmSession::Exception(500,"Failed to fetch credit");
    }
    return;
  }
  else if(m_credit == 0) {
    DBG("No credit left\n");
    m_state = CC_Teardown;
    if(dlg.reply(req, 402, "Unsufficient Credit") != 0) {
      throw AmSession::Exception(500,"Failed to reply 402");
    }
    setStopped();
    //throw AmSession::Exception(402, "Unsufficient Credit");
  }
  else {
    DBG("Credit for UUID %s is %d seconds, calling %s now\n", m_uuid.c_str(), m_credit, req.r_uri.c_str());
    m_state = CC_Dialing;

    if(dlg.reply(req, 101, "Connecting") != 0) {
      throw AmSession::Exception(500,"Failed to reply 101");
    }

    invite_req = req;
    dlg.updateStatus(req);
    recvd_req.insert(std::make_pair(req.cseq,req));

    connectCallee("<" + m_ruri + ">", m_proxy + ";sw_prepaid", true);
    sip_relay_only = true;
  }
}


void SWPrepaidSIPDialog::process(AmEvent* ev)
{
  AmAudioEvent* audio_ev = dynamic_cast<AmAudioEvent*>(ev);
  if(audio_ev && (audio_ev->event_id == AmAudioEvent::noAudio) && m_state == CC_Teardown) {
    DBG("SWPrepaidSIPDialog::process: Playlist is empty!\n");
    terminateLeg();

    ev->processed = true;
    return;
  }

  AmPluginEvent* plugin_event = dynamic_cast<AmPluginEvent*>(ev);
  if(plugin_event && plugin_event->name == "timer_timeout") {
    int timer_id = plugin_event->data.get(0).asInt();
    if (timer_id == TIMERID_CREDIT_TIMEOUT) {
      DBG("timer timeout, no more credit\n");
      stopAccounting();
      terminateOtherLeg();
      terminateLeg();

      ev->processed = true;
      return;
    }
  }

  AmB2BCallerSession::process(ev);
}


bool SWPrepaidSIPDialog::onOtherReply(const AmSipReply& reply)
{
  bool ret = false;

  if (m_state == CC_Dialing) {
    if (reply.code < 200) {
      DBG("Callee is trying... code %d\n", reply.code);
    }
    else if(reply.code < 300) {
      if(getCalleeStatus()  == Connected) {
        m_state = CC_Connected;
        startAccounting();
        setInOut(NULL, NULL);

        // set the call timer
        AmArg di_args,ret;
        di_args.push(TIMERID_CREDIT_TIMEOUT);
        di_args.push(m_credit);  // in seconds
        di_args.push(dlg.local_tag.c_str());
        m_user_timer->invoke("setTimer", di_args, ret);
      }
    }
    else if(reply.code == 487 && dlg.getStatus() == AmSipDialog::Pending) {
      DBG("Canceling leg A on 487 from B");
      dlg.reply(m_localreq, 487, "Call terminated");
      setStopped();
      ret = true;
    }
    else if (reply.code >= 300 && dlg.getStatus() == AmSipDialog::Connected) {
      DBG("Callee final error in connected state with code %d\n",reply.code);
      terminateLeg();
    }
    else {
      DBG("Callee final error with code %d\n",reply.code);
      AmB2BCallerSession::onOtherReply(reply);
    }
  }
  return ret;
}


void SWPrepaidSIPDialog::onOtherBye(const AmSipRequest& req)
{
  stopAccounting();
  AmB2BCallerSession::onOtherBye(req);
}


void SWPrepaidSIPDialog::onBye(const AmSipRequest& req)
{
  if (m_state == CC_Connected) {
    stopAccounting();
  }
  terminateOtherLeg();
  setStopped();
}


void SWPrepaidSIPDialog::onCancel()
{
  if(dlg.getStatus() == AmSipDialog::Pending) {
    DBG("Wait for leg B to terminate");
  }
  else {
    DBG("Canceling leg A on CANCEL since dialog is not pending");
    dlg.reply(m_localreq, 487, "Call terminated");
    setStopped();
  }
}


void SWPrepaidSIPDialog::startAccounting()
{
  gettimeofday(&m_acc_start, NULL);
  DBG("start accounting at %ld\n", m_acc_start.tv_sec);
}


void SWPrepaidSIPDialog::stopAccounting()
{
  if(m_acc_start.tv_sec != 0 || m_acc_start.tv_usec != 0) {
    struct timeval now;
    gettimeofday(&now, NULL);
    timersub(&now, &m_acc_start, &now);
    if(now.tv_usec > 500000)
      now.tv_sec++;
    DBG("Call lasted %ld seconds\n", now.tv_sec);

    AmArg di_args,ret;
    di_args.push(m_uuid.c_str());
    di_args.push((int)m_starttime);
    di_args.push((int)now.tv_sec);
    di_args.push(m_dest.c_str());
    m_cc_acc->invoke("subtractCredit", di_args, ret);
  }
}
