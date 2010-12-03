/*
 * Copyright (C) 2010 Stefan Sayer
 * 
 * Development of this module was sponsored by TelTech Systems Inc
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

#include "ModGroups.h"
#include "log.h"
#include "AmUtils.h"

#include "DSMSession.h"
#include "AmSession.h"
#include "AmSessionContainer.h"

SC_EXPORT(MOD_CLS_NAME);

AmMutex GroupsModule::groups_mut;
GroupMap GroupsModule::groups;
GroupMap GroupsModule::groups_rev;


MOD_ACTIONEXPORT_BEGIN(MOD_CLS_NAME) {

  DEF_CMD("groups.join", GroupsJoinAction);
  DEF_CMD("groups.leave", GroupsLeaveAction);
  DEF_CMD("groups.leaveAll", GroupsLeaveAllAction);
  // DEF_CMD("groups.get", GroupsGetAction);
  // DEF_CMD("groups.getMembers", GroupsGetMembersAction);
  DEF_CMD("groups.postEvent", GroupsPostEventAction);

} MOD_ACTIONEXPORT_END;

MOD_CONDITIONEXPORT_NONE(MOD_CLS_NAME);

void GroupsModule::onBeforeDestroy(DSMSession* sc_sess, AmSession* sess) {
  leave_all_groups(sess->getLocalTag());
}

void GroupsModule::leave_all_groups(const string& ltag) {
  GroupsModule::groups_mut.lock();

  GroupMap::iterator it = GroupsModule::groups_rev.find(ltag);
  if (it != GroupsModule::groups_rev.end()) {
    set<string>& active_groups = it->second;
    for (set<string>::iterator ag_it =
	   active_groups.begin(); ag_it != active_groups.end(); ag_it++) {
      GroupsModule::groups[*ag_it].erase(ltag);
      if (GroupsModule::groups[*ag_it].empty()) {
	DBG("clearing empty group '%s'\n", ag_it->c_str());
	GroupsModule::groups.erase(*ag_it);
      }
    }
    GroupsModule::groups_rev.erase(it);
  }

  GroupsModule::groups_mut.unlock();
}

EXEC_ACTION_START(GroupsJoinAction) {
  string groupname = resolveVars(arg, sess, sc_sess, event_params);
  DBG("call '%s' joining group '%s'\n",
      sess->getLocalTag().c_str(), groupname.c_str());

  GroupsModule::groups_mut.lock();
  GroupsModule::groups[groupname].insert(sess->getLocalTag());
  GroupsModule::groups_rev[sess->getLocalTag()].insert(groupname);
  GroupsModule::groups_mut.unlock();
} EXEC_ACTION_END;

EXEC_ACTION_START(GroupsLeaveAction) {
  string groupname = resolveVars(arg, sess, sc_sess, event_params);
  string ltag = sess->getLocalTag();
  DBG("call '%s' leaving group '%s'\n",
      ltag.c_str(), groupname.c_str());

  GroupsModule::groups_mut.lock();

  GroupMap::iterator it = GroupsModule::groups.find(groupname);
  if (it != GroupsModule::groups.end()) {
    it->second.erase(ltag);
    if (it->second.empty()) {
      DBG("clearing empty group '%s'\n", groupname.c_str());
      GroupsModule::groups.erase(it);
    }
  }

  it = GroupsModule::groups_rev.find(ltag);
  if (it != GroupsModule::groups_rev.end()) {
    it->second.erase(groupname);
    if (it->second.empty()) {
      DBG("call '%s' in no group any more\n", ltag.c_str());
      GroupsModule::groups_rev.erase(it);
    }
  }
  GroupsModule::groups_mut.unlock();
} EXEC_ACTION_END;

EXEC_ACTION_START(GroupsLeaveAllAction) {
  string ltag = sess->getLocalTag();
  DBG("call '%s' leaving all groups\n", ltag.c_str());

  GroupsModule::leave_all_groups(ltag);
} EXEC_ACTION_END;

CONST_ACTION_2P(GroupsPostEventAction, ',', true);
EXEC_ACTION_START(GroupsPostEventAction) {
  string groupname = resolveVars(par1, sess, sc_sess, event_params);
  string var = resolveVars(par2, sess, sc_sess, event_params);
  map<string, string> ev_params;

  if (!var.empty()) {
    if (var == "var")
      ev_params = sc_sess->var;
    else {
      vector<string> vars = explode(var, ";");
      for (vector<string>::iterator it =
	     vars.begin(); it != vars.end(); it++)
	ev_params[*it] = sc_sess->var[*it];
    }
  }


  DBG("posting event to group '%s'\n", groupname.c_str());
  GroupsModule::groups_mut.unlock();
  GroupMap::iterator grp = GroupsModule::groups.find(groupname);
  bool posted = false;
  if (grp != GroupsModule::groups.end()) {
    for (set<string>::iterator it =
	   grp->second.begin(); it != grp->second.end(); it++) {
      if (*it == sess->getLocalTag())
	continue; // don't post to myself

      DSMEvent* ev = new DSMEvent();
      ev->params = ev_params;
      if (!AmSessionContainer::instance()->postEvent(*it, ev)) {
	DBG("could not post to call '%s'\n", it->c_str());
      } else {
	DBG("posted event to call '%s'\n", it->c_str());
	posted = true;
      }
    }
  }
  GroupsModule::groups_mut.unlock();

  if (posted) {
    sc_sess->CLR_ERRNO;
  } else {
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("event could not be posted\n");
  }

} EXEC_ACTION_END;
