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
#include "log.h"

#include "AmUtils.h"
#include "AmSessionContainer.h"
#include "ampi/MonitoringAPI.h"

#include "DSMSession.h"
#include "DSMCoreModule.h"
#include "AmSession.h"

#include "ModMonitoring.h"

SC_EXPORT(MonitoringModule);

void splitCmd(const string& from_str, 
	      string& cmd, string& params) {
  size_t b_pos = from_str.find('(');
  if (b_pos != string::npos) {
    cmd = from_str.substr(0, b_pos);
    params = from_str.substr(b_pos + 1, from_str.rfind(')') - b_pos -1);
  } else 
    cmd = from_str;  
}

DSMAction* MonitoringModule::getAction(const string& from_str) {
  string cmd;
  string params;
  splitCmd(from_str, cmd, params);

#define DEF_CMD(cmd_name, class_name) \
				      \
  if (cmd == cmd_name) {	      \
    class_name * a =		      \
      new class_name(params);	      \
    a->name = from_str;		      \
    return a;			      \
  }

  DEF_CMD("monitoring.log", MonLogAction);
  DEF_CMD("monitoring.logAdd", MonLogAddAction);
  DEF_CMD("monitoring.logVars", MonLogVarsAction);

  return NULL;
}

DSMCondition* MonitoringModule::getCondition(const string& from_str) {
  return NULL;
}

#define GET_SCSESSION()					 \
  DSMSession* sc_sess = dynamic_cast<DSMSession*>(sess); \
  if (!sc_sess) {					 \
    ERROR("wrong session type\n");			 \
    return false;					 \
  }


CONST_TwoParAction(MonLogAction, ",", true);
bool MonLogAction::execute(AmSession* sess, 
			   DSMCondition::EventType event,
			   map<string,string>* event_params) {
  GET_SCSESSION();

  string prop = resolveVars(par1, sess, sc_sess, event_params);
  string val  = resolveVars(par2, sess, sc_sess, event_params);
  
  MONITORING_LOG(sess->getLocalTag().c_str(), prop.c_str(), val.c_str());

  return false;
}

CONST_TwoParAction(MonLogAddAction, ",", true);
bool MonLogAddAction::execute(AmSession* sess, 
			   DSMCondition::EventType event,
			   map<string,string>* event_params) {
  GET_SCSESSION();

  string prop = resolveVars(par1, sess, sc_sess, event_params);
  string val  = resolveVars(par2, sess, sc_sess, event_params);
  
  MONITORING_LOG_ADD(sess->getLocalTag().c_str(), prop.c_str(), val.c_str());

  return false;
}

bool MonLogVarsAction::execute(AmSession* sess, 
			       DSMCondition::EventType event,
			       map<string,string>* event_params) {
  GET_SCSESSION();

  AmArg di_args,ret;
  di_args.push(AmArg(sess->getLocalTag().c_str()));

  for (map<string,string>::iterator it=
	 sc_sess->var.begin(); it != sc_sess->var.end();it++) {
    di_args.push(it->first.c_str());
    di_args.push(it->second.c_str());
  }
  AmSessionContainer::monitoring_di->invoke("log", di_args, ret);

  return false;
}



