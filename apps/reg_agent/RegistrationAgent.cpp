/*
 * $Id$
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

#include "RegistrationAgent.h"
#include "AmConfig.h"
#include "AmUtils.h"
#include "AmPlugIn.h"

#include "sems.h"
#include "log.h"

#include <unistd.h>
#include "ampi/SIPRegistrarClientAPI.h"

#define MOD_NAME "reg_agent"

EXPORT_SESSION_FACTORY(RegistrationAgentFactory,MOD_NAME);

RegistrationAgentFactory::RegistrationAgentFactory(const string& _app_name)
  : AmSessionFactory(_app_name)
{
}

int RegistrationAgentFactory::onLoad()
{
  AmConfigReader cfg;
  if(cfg.loadFile(AmConfig::ModConfigPath + string(MOD_NAME ".conf")))
    return -1;

  // get application specific global parameters
  configureModule(cfg);

  // stay backwards compatible
  RegInfo ri; 
  ri.domain = cfg.getParameter("domain","");
  ri.user = cfg.getParameter("user","");
  ri.display_name = cfg.getParameter("display_name","");
  ri.auth_user = cfg.getParameter("auth_user","");
  ri.passwd = cfg.getParameter("pwd","");
  ri.proxy = cfg.getParameter("proxy","");

  if (!ri.domain.length() || 
      !ri.user.length() || 
      !ri.display_name.length() || 
      !ri.auth_user.length() || 
      !ri.passwd.length()) {
    ERROR("Account for registration not correctly configured.\n");
    ERROR("RegistrationAgent will not register any accounts.\n");
    return 0;
  }

  DBG("Adding registration #%d (%s %s %s %s %s)\n", 0, 
      ri.domain.c_str(), ri.user.c_str(), ri.display_name.c_str(), ri.auth_user.c_str(), ri.proxy.c_str());

  dialer.add_reg(ri);

  unsigned int ri_index = 1;
  while (ri_index < 100) {
    RegInfo ri; 
    ri.domain = cfg.getParameter("domain"+int2str(ri_index),"");
    ri.user = cfg.getParameter("user"+int2str(ri_index),"");
    ri.display_name = cfg.getParameter("display_name"+int2str(ri_index),"");
    ri.auth_user = cfg.getParameter("auth_user"+int2str(ri_index),"");
    ri.passwd = cfg.getParameter("pwd"+int2str(ri_index),"");
    ri.proxy = cfg.getParameter("proxy"+int2str(ri_index),"");
      
    if (!ri.domain.length() || 
	!ri.user.length() || 
	!ri.display_name.length() || 
	!ri.auth_user.length() || 
	!ri.passwd.length())
      break;
	
    DBG("Adding registration #%d (%s %s %s %s %s)\n", ri_index, 
	ri.domain.c_str(), ri.user.c_str(), ri.display_name.c_str(), ri.auth_user.c_str(), ri.proxy.c_str());
    dialer.add_reg(ri);
    ri_index++;
  }

  dialer.start();

  return 0;
}

void RegistrationAgentFactory::postEvent(AmEvent* ev) {
  dialer.postEvent(ev);
}

AmSession* RegistrationAgentFactory::onInvite(const AmSipRequest& req)
{
  return NULL;
}


void RegThread::add_reg(const RegInfo& ri) {
  registrations.push_back(ri);
}

void RegThread::create_registration(RegInfo& ri) {
  AmDynInvokeFactory* di_f = AmPlugIn::instance()->getFactory4Di("registrar_client");
  if (di_f == NULL) {
    ERROR("unable to get a registrar_client\n");
  } else {
    AmDynInvoke* uac_auth_i = di_f->getInstance();
    if (uac_auth_i!=NULL) {

      DBG("calling createRegistration\n");
      AmArg di_args, reg_handle;
      di_args.push(ri.domain.c_str());
      di_args.push(ri.user.c_str());
      di_args.push(ri.display_name.c_str()); // display name
      di_args.push(ri.auth_user.c_str());  // auth_user
      di_args.push(ri.passwd.c_str());    // pwd
      di_args.push("reg_agent"); //sess_link
      di_args.push(ri.proxy.c_str()); 
			
      uac_auth_i->invoke("createRegistration", di_args, reg_handle);
      if (reg_handle.size()) 
	ri.handle = reg_handle.get(0).asCStr();
    }
  }
}

bool RegThread::check_registration(const RegInfo& ri) {
  if (!ri.handle.length())
    return false;
  AmDynInvokeFactory* di_f = AmPlugIn::instance()->getFactory4Di("registrar_client");
  if (di_f == NULL) {
    ERROR("unable to get a registrar_client\n");
  } else {
    AmDynInvoke* uac_auth_i = di_f->getInstance();
    if (uac_auth_i!=NULL) {

      AmArg di_args, res;
      di_args.push(ri.handle.c_str());
      uac_auth_i->invoke("getRegistrationState", di_args, res);
      if (res.size()) {
	if (!res.get(0).asInt())
	  return false; // does not exist
	int state = res.get(1).asInt();
	int expires = res.get(2).asInt();
	DBG("Got state %s with expires %us for registration.\n", 
	    getSIPRegistationStateString(state), expires);
	if (state == 2) // expired ... FIXME: add values from API here
	  return false;
	// else pending or active
	return true;
      }
    }
  }
  return false;
}


void RegThread::run() {
  DBG("registrar client started.\n");
  sleep(2); // wait for sems to completely start up

  while (true) {
    for (vector<RegInfo>::iterator it = registrations.begin(); 
	 it != registrations.end(); it++) {
      if (!check_registration(*it)) {
	// todo: this is very crude... should adjust retry time
	DBG("Registration %d does not exist or timeout. Creating registration.\n",
	    (int)(it - registrations.begin()));
	create_registration(*it);
      }
    }
    sleep(10); // 10 seconds
  }
		
}

void RegThread::on_stop() {
  DBG("not stopping...\n");
}

void RegThread::postEvent(AmEvent* ev) {
  DBG("received registration event.\n"); 
  // TODO: handle events instead of re-try
  delete ev;
}

