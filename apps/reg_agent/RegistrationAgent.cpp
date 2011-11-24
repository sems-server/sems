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

#include "RegistrationAgent.h"
#include "AmConfig.h"
#include "AmUtils.h"
#include "AmPlugIn.h"

#include "sems.h"
#include "log.h"

#include <unistd.h>
#include "ampi/SIPRegistrarClientAPI.h"

#define MOD_NAME "reg_agent"

#define CFG_PARAM_DOMAIN  "domain"
#define CFG_PARAM_USER    "user"
#define CFG_PARAM_DISPLAY "display_name"
#define CFG_PARAM_AUTH    "auth_user"
#define CFG_PARAM_PASS    "pwd"
#define CFG_PARAM_PROXY   "proxy"
#define CFG_PARAM_CONTACT "contact"

#define MAX_ACCOUNTS      100


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

  int i = 0;
  string idx_str;
  do {
    RegInfo ri;
    ri.domain = cfg.getParameter(CFG_PARAM_DOMAIN+idx_str,"");
    ri.user = cfg.getParameter(CFG_PARAM_USER+idx_str,"");
    ri.display_name = cfg.getParameter(CFG_PARAM_DISPLAY+idx_str,"");
    ri.auth_user = cfg.getParameter(CFG_PARAM_AUTH+idx_str,"");
    ri.passwd = cfg.getParameter(CFG_PARAM_PASS+idx_str,"");
    ri.proxy = cfg.getParameter(CFG_PARAM_PROXY+idx_str,"");
    ri.contact = cfg.getParameter(CFG_PARAM_CONTACT+idx_str,"");

    if (!ri.domain.length() || !ri.user.length()) {
      // not including the passwd: might be IP based registration
      // not including the display name: allow user to skip it
      DBG("no mandatory config parameters '" CFG_PARAM_DOMAIN "' and '"
        CFG_PARAM_USER "' provided for entry #%d; configuration halted.\n", i);
      break;
    }

    if (!ri.auth_user.length()) // easier to config
      ri.auth_user = ri.user;

    dialer.add_reg(ri);
    DBG("Adding registration account #%d (%s %s %s %s %s %s)\n", i,
        ri.domain.c_str(), ri.user.c_str(), ri.display_name.c_str(), 
        ri.auth_user.c_str(), ri.proxy.c_str(), ri.contact.c_str());

    i ++;
    idx_str = int2str(i);
  } while (i < MAX_ACCOUNTS);

  if (i <= 0) {
    WARN("no complete account provided: '" MOD_NAME "' module remains "
        "inactive, which might not be what you want!\n");
  } else {
    dialer.start();
  }

  return 0;
}

void RegistrationAgentFactory::postEvent(AmEvent* ev) {
  dialer.postEvent(ev);
}

AmSession* RegistrationAgentFactory::onInvite(const AmSipRequest& req, const string& app_name,
					      const map<string,string>& app_params)
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
    AmDynInvoke* registrar_client_i = di_f->getInstance();
    if (registrar_client_i!=NULL) {

      DBG("calling createRegistration\n");
      AmArg di_args, reg_handle;
      di_args.push(ri.domain.c_str());
      di_args.push(ri.user.c_str());
      di_args.push(ri.display_name.c_str()); // display name
      di_args.push(ri.auth_user.c_str());    // auth_user
      di_args.push(ri.passwd.c_str());       // pwd
      di_args.push("reg_agent");             //sess_link
      di_args.push(ri.proxy.c_str());
      di_args.push(ri.contact.c_str());
			
      registrar_client_i->invoke("createRegistration", di_args, reg_handle);
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
    AmDynInvoke* registrar_client_i = di_f->getInstance();
    if (registrar_client_i!=NULL) {

      AmArg di_args, res;
      di_args.push(ri.handle.c_str());
      registrar_client_i->invoke("getRegistrationState", di_args, res);
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

