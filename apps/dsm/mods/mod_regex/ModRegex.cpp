/*
 * Copyright (C) 2012 Stefan Sayer
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

#include "ModRegex.h"

#include "log.h"
#include "AmUtils.h"
#include "AmConfigReader.h"

SC_EXPORT(MOD_CLS_NAME);

map<string, TsRegex> MOD_CLS_NAME::regexes;

MOD_ACTIONEXPORT_BEGIN(MOD_CLS_NAME) {

  DEF_CMD("regex.compile", SCCompileRegexAction);
  DEF_CMD("regex.match", SCExecRegexAction);
  DEF_CMD("regex.clear", SCClearRegexAction);
} MOD_ACTIONEXPORT_END;

MOD_CONDITIONEXPORT_BEGIN(MOD_CLS_NAME) {

  if (cmd == "regex.match") {
    return new SCExecRegexCondition(params, false);
  }

} MOD_CONDITIONEXPORT_END;

int MOD_CLS_NAME::preload() {
   AmConfigReader cfg;
   if(cfg.loadPluginConf(MOD_NAME)) {
     INFO("no module configuration for '%s' found, not preloading regular expressions\n",
	  MOD_NAME);
     return 0;
   }

   bool failed = false;
   for (std::map<string,string>::const_iterator it =
	  cfg.begin(); it != cfg.end(); it++) {
     if (add_regex(it->first, it->second)) {
       ERROR("compiling regex '%s' for '%s'\n",
	     it->second.c_str(), it->first.c_str());
       failed = true;
     } else {
       DBG("compiled regex '%s' as '%s'\n", it->second.c_str(), it->first.c_str());
     }
   }

   return failed? -1 : 0;
}

int MOD_CLS_NAME::add_regex(const string& r_name, const string& r_reg) {
  if (regexes[r_name].regcomp(r_reg.c_str(), REG_NOSUB | REG_EXTENDED)) {
    ERROR("compiling '%s' for regex '%s'\n", r_reg.c_str(), r_name.c_str());
    regexes.erase(r_name);
    return -1;
  }
  return 0;
}


CONST_CONDITION_2P(SCExecRegexCondition, ',', false);
MATCH_CONDITION_START(SCExecRegexCondition) {
  DBG("checking condition '%s' '%s'\n", par1.c_str(), par2.c_str());
  return true;
} MATCH_CONDITION_END;


CONST_ACTION_2P(SCCompileRegexAction, ',', false);
EXEC_ACTION_START(SCCompileRegexAction) {
  string rname = resolveVars(par1, sess, sc_sess, event_params);
  string rval = par2; //resolveVars(par2, sess, sc_sess, event_params);
  DBG("compiling '%s' for regex '%s'\n", rval.c_str(), rname.c_str());

  if (MOD_CLS_NAME::add_regex(rname, rval)) {
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    ERROR("compiling '%s' for regex '%s'\n", rval.c_str(), rname.c_str());
  }
} EXEC_ACTION_END;

CONST_ACTION_2P(SCExecRegexAction, ',', false);
EXEC_ACTION_START(SCExecRegexAction) {
  string rname = resolveVars(par1, sess, sc_sess, event_params);
  string val = resolveVars(par2, sess, sc_sess, event_params);
  DBG("matching '%s' on regex '%s'\n", val.c_str(), rname.c_str());
  map<string, TsRegex>::iterator it=MOD_CLS_NAME::regexes.find(rname);
  if (it == MOD_CLS_NAME::regexes.end()) {
    ERROR("regex '%s' not found for matching '%s'\n", rname.c_str(), val.c_str());
    EXEC_ACTION_STOP;
  }

  int res = it->second.regexec(val.c_str(), 1, NULL, 0);
  if (!res) {
    // yeah side effects
    sc_sess->var["regex.match"] = "1";
  } else {
    sc_sess->var["regex.match"] = "0";
  }
} EXEC_ACTION_END;

EXEC_ACTION_START(SCClearRegexAction) {
  string r_name = resolveVars(arg, sess, sc_sess, event_params);
  DBG("clearing  regex '%s'\n", r_name.c_str());
  MOD_CLS_NAME::regexes.erase(r_name);
} EXEC_ACTION_END;

TsRegex::TsRegex()
  : i(false) { }

TsRegex::~TsRegex()
{
  if (i) {
    regfree(&reg);
  }
}

int TsRegex::regcomp(const char *regex, int cflags) {
  m.lock();
  if (i) {
    regfree(&reg);
  }
  int res = ::regcomp(&reg, regex, cflags);
  if (!res)
    i=true;
  m.unlock();
  return res;
}

int TsRegex::regexec(const char *_string, size_t nmatch,
		     regmatch_t pmatch[], int eflags) {
  if (!i) {
    ERROR("uninitialized regex when matching '%s'\n", _string);
    return -1;
  }
  m.lock();
  int res = ::regexec(&reg, _string, nmatch, pmatch, eflags);
  m.unlock();
  return res;
}
