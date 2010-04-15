/*
 * $Id$
 *
 * Copyright (C) 2002-2003 Fhg Fokus
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

#include "AmApi.h"
#include "AmSession.h"
#include "AmConfig.h"
#include "AmAudio.h"
#include "AmConfigReader.h"
#include "log.h"

#include "Echo.h"
#include "AmAudioEcho.h"

#include "AmPlugIn.h"

EXPORT_SESSION_FACTORY(EchoFactory,"echo");

#define STAR_SWITCHES_PLAYOUTBUFFER

EchoFactory::EchoFactory(const string& _app_name)
  : AmSessionFactory(_app_name),
    session_timer_f(NULL)
{}

int EchoFactory::onLoad()
{
  bool useSessionTimer = false;
    
  if(conf.loadFile(AmConfig::ModConfigPath + string(MODULE_NAME)+ ".conf")){
    WARN("Could not open " MODULE_NAME ".conf\n");
    WARN("assuming that default values are fine\n");
  }
  else {
    if(conf.hasParameter("enable_session_timer") &&
       (conf.getParameter("enable_session_timer") == string("yes")) ){
      
      useSessionTimer = true;
    }
  }
  
  if(useSessionTimer){
    session_timer_f = AmPlugIn::instance()->getFactory4Seh("session_timer");
    //    DBG("session_timer_f == 0x%.16lX\n",(unsigned long)session_timer_f);
    if(session_timer_f == NULL){
      ERROR("Could not load the session_timer module: disabling session timers.\n");
      //return -1;
    }
  }
  
  return 0;
}

AmSession* EchoFactory::onInvite(const AmSipRequest& req)
{
  AmSession* s = new EchoDialog();
  
  if (NULL != session_timer_f) {
    AmSessionEventHandler* h = session_timer_f->getHandler(s);
    
    if(h->configure(conf)){
      ERROR("Could not configure the session timer: disabling session timers.\n");
      delete h;
    } else {
      s->addHandler(h);
    }
  }

  return s;
}

EchoDialog::EchoDialog() 
  : playout_type(ADAPTIVE_PLAYOUT)
{
	
}

EchoDialog::~EchoDialog()
{
}

void EchoDialog::onSessionStart(const AmSipRequest& req)
{
  RTPStream()->setPlayoutType(playout_type);
  setInOut(&echo,&echo);
}

void EchoDialog::onSessionStart(const AmSipReply& req)
{
  RTPStream()->setPlayoutType(playout_type);
  setInOut(&echo,&echo);
}

void EchoDialog::onBye(const AmSipRequest& req)
{
  setStopped();
}

void EchoDialog::onDtmf(int event, int duration)
{
#ifdef STAR_SWITCHES_PLAYOUTBUFFER
  if (event == 10) {   
    const char* pt = "simple (fifo) playout buffer";
    if (playout_type == SIMPLE_PLAYOUT) {
      playout_type = ADAPTIVE_PLAYOUT;
      pt = "adaptive playout buffer";
    } else if (playout_type == ADAPTIVE_PLAYOUT) {
      pt = "adaptive jitter buffer";
      playout_type = JB_PLAYOUT;
    } else
      playout_type = SIMPLE_PLAYOUT;
    DBG("received *. set playout technique to %s.\n", pt);
		
    RTPStream()->setPlayoutType(playout_type);
  }
#endif
}
