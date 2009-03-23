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
#include "ModSys.h"
#include "log.h"
#include "AmUtils.h"

#include "DSMSession.h"
#include "AmSession.h"
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "DSMCoreModule.h"

SC_EXPORT(SCSysModule);

SCSysModule::SCSysModule() {
}

SCSysModule::~SCSysModule() {
}

void splitCmd(const string& from_str, 
	      string& cmd, string& params) {
  size_t b_pos = from_str.find('(');
  if (b_pos != string::npos) {
    cmd = from_str.substr(0, b_pos);
    params = from_str.substr(b_pos + 1, from_str.rfind(')') - b_pos -1);
  } else 
    cmd = from_str;  
}

DSMAction* SCSysModule::getAction(const string& from_str) {
  string cmd;
  string params;
  splitCmd(from_str, cmd, params);

  DEF_CMD("sys.mkdir", SCMkDirAction);

  return NULL;
}

DSMCondition* SCSysModule::getCondition(const string& from_str) {
  string cmd;
  string params;
  splitCmd(from_str, cmd, params);

  if (cmd == "sys.file_exists") {
    return new FileExistsCondition(params, false);
  }

  // ahem... missing not? 
  if (cmd == "sys.file_not_exists") {
    return new FileExistsCondition(params, true);
  }

  return NULL;
}

MATCH_CONDITION_START(FileExistsCondition) {
  DBG("checking file '%s'\n", arg.c_str());
  string fname = resolveVars(arg, sess, sc_sess, event_params);
  bool ex =  file_exists(fname);
  DBG("file '%s' %s\n", fname.c_str(), ex?"exists":"does not exist");
  if (inv) {
    DBG("returning %s\n", (!ex)?"true":"false");
    return !ex;
  }  else {
    DBG("returning %s\n", (ex)?"true":"false");
    return ex;
  }
} MATCH_CONDITION_END;


EXEC_ACTION_START(SCMkDirAction) {
  string d = resolveVars(arg, sess, sc_sess, event_params);
  DBG("mkdir '%s'\n", d.c_str());
  if (mkdir(d.c_str(),  S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) {
    ERROR("kmdir failed for '%s': %s\n", 
	  d.c_str(), strerror(errno));
  }
} EXEC_ACTION_END;

