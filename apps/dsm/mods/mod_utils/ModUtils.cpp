/*
 * $Id$
 *
 * Copyright (C) 2009 Teltech Systems Inc.
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
#include "ModUtils.h"
#include "log.h"
#include "AmUtils.h"
#include <math.h>
#include <stdlib.h>
#include <time.h>

#include "DSMSession.h"
#include "AmSession.h"

SC_EXPORT(MOD_CLS_NAME);

MOD_ACTIONEXPORT_BEGIN(MOD_CLS_NAME) {

  DEF_CMD("utils.playCountRight", SCUPlayCountRightAction);
  DEF_CMD("utils.playCountLeft",  SCUPlayCountLeftAction);
  DEF_CMD("utils.getNewId", SCGetNewIdAction);
  DEF_CMD("utils.spell", SCUSpellAction);
  DEF_CMD("utils.rand", SCURandomAction);
  DEF_CMD("utils.srand", SCUSRandomAction);


} MOD_ACTIONEXPORT_END;

MOD_CONDITIONEXPORT_NONE(MOD_CLS_NAME);

bool utils_play_count(DSMSession* sc_sess, unsigned int cnt, 
		      const string& basedir, const string& suffix, bool right) {
  
  if (cnt <= 20) {
    sc_sess->playFile(basedir+int2str(cnt)+suffix, false);
    return false;
  }
  
  for (int i=9;i>1;i--) {
    div_t num = div(cnt, (int)exp10(i));  
    if (num.quot) {
      sc_sess->playFile(basedir+int2str(int(num.quot * exp10(i)))+suffix, false);
    }
    cnt = num.rem;
  }

  if (!cnt)
    return false;

  if ((cnt <= 20) || (!(cnt%10))) {
    sc_sess->playFile(basedir+int2str(cnt)+suffix, false);
    return false;
  }

  div_t num = div(cnt, 10);
  if (right) { 
   // language has single digits before 10s
    sc_sess->playFile(basedir+int2str(num.quot * 10)+suffix, false);
    sc_sess->playFile(basedir+("x"+int2str(num.rem))+suffix, false);    
  } else {
    // language has single digits before 10s
    sc_sess->playFile(basedir+("x"+int2str(num.rem))+suffix, false);    
    sc_sess->playFile(basedir+int2str(num.quot * 10)+suffix, false);
  }

  return false;
}

CONST_ACTION_2P(SCUPlayCountRightAction, ',', true);
EXEC_ACTION_START(SCUPlayCountRightAction) {
  string cnt_s = resolveVars(par1, sess, sc_sess, event_params);
  string basedir = resolveVars(par2, sess, sc_sess, event_params);

  unsigned int cnt = 0;
  if (str2i(cnt_s,cnt)) {
    ERROR("could not parse count '%s'\n", cnt_s.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("could not parse count '"+cnt_s+"'\n");
    return false;
  }

  utils_play_count(sc_sess, cnt, basedir, ".wav", true);

  sc_sess->CLR_ERRNO;
} EXEC_ACTION_END;


CONST_ACTION_2P(SCUPlayCountLeftAction, ',', true);
EXEC_ACTION_START(SCUPlayCountLeftAction) {
  string cnt_s = resolveVars(par1, sess, sc_sess, event_params);
  string basedir = resolveVars(par2, sess, sc_sess, event_params);

  unsigned int cnt = 0;
  if (str2i(cnt_s,cnt)) {
    ERROR("could not parse count '%s'\n", cnt_s.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("could not parse count '"+cnt_s+"'\n");
    return false;
  }

  utils_play_count(sc_sess, cnt, basedir, ".wav", false);
  sc_sess->CLR_ERRNO;
} EXEC_ACTION_END;

EXEC_ACTION_START(SCGetNewIdAction) {
  string d = resolveVars(arg, sess, sc_sess, event_params);
  sc_sess->var[d]=AmSession::getNewId();
} EXEC_ACTION_END;

CONST_ACTION_2P(SCURandomAction, ',', true);
EXEC_ACTION_START(SCURandomAction) {
  string varname = resolveVars(par1, sess, sc_sess, event_params);
  string modulo_str = resolveVars(par2, sess, sc_sess, event_params);

  unsigned int modulo = 0;
  if (modulo_str.length()) 
    str2i(modulo_str, modulo);
  
  if (modulo)
    sc_sess->var[varname]=int2str(rand()%modulo);
  else
    sc_sess->var[varname]=int2str(rand());

  DBG("Generated random $%s=%s\n", varname.c_str(), sc_sess->var[varname].c_str());
} EXEC_ACTION_END;

EXEC_ACTION_START(SCUSRandomAction) {
  srand(time(0));
} EXEC_ACTION_END;

CONST_ACTION_2P(SCUSpellAction, ',', true);
EXEC_ACTION_START(SCUSpellAction) {
  string basedir = resolveVars(par2, sess, sc_sess, event_params);

  string play_string = resolveVars(par1, sess, sc_sess, event_params);
  DBG("spelling '%s'\n", play_string.c_str());
  for (size_t i=0;i<play_string.length();i++)
    sc_sess->playFile(basedir+play_string[i]+".wav", false);

} EXEC_ACTION_END;
