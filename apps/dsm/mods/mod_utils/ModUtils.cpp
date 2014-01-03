/*
 * Copyright (C) 2009 Teltech Systems Inc.
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
#include "ModUtils.h"
#include "log.h"
#include "AmUtils.h"
#include <math.h>
#include <stdlib.h>
#include <time.h>

#include "DSMSession.h"
#include "AmSession.h"
#include "AmPlaylist.h"

SC_EXPORT(MOD_CLS_NAME);

MOD_ACTIONEXPORT_BEGIN(MOD_CLS_NAME) {

  DEF_CMD("utils.playCountRight", SCUPlayCountRightAction);
  DEF_CMD("utils.playCountLeft",  SCUPlayCountLeftAction);
  DEF_CMD("utils.getNewId", SCGetNewIdAction);
  DEF_CMD("utils.spell", SCUSpellAction);
  DEF_CMD("utils.rand", SCURandomAction);
  DEF_CMD("utils.srand", SCUSRandomAction);
  DEF_CMD("utils.add", SCUSAddAction);
  DEF_CMD("utils.sub", SCUSSubAction);
  DEF_CMD("utils.int", SCUIntAction);
  DEF_CMD("utils.splitStringCR", SCUSplitStringAction);
  DEF_CMD("utils.escapeCRLF", SCUEscapeCRLFAction);
  DEF_CMD("utils.unescapeCRLF", SCUUnescapeCRLFAction);
  DEF_CMD("utils.playRingTone", SCUPlayRingToneAction);

} MOD_ACTIONEXPORT_END;

MOD_CONDITIONEXPORT_NONE(MOD_CLS_NAME);

bool utils_play_count(DSMSession* sc_sess, unsigned int cnt, 
		      const string& basedir, const string& suffix, bool right) {
  
  if (cnt <= 20) {
    sc_sess->playFile(basedir+int2str(cnt)+suffix, false);
    return false;
  }
  
  for (int i=9;i>1;i--) {
    div_t num = div(cnt, (int)pow(10.,i));  
    if (num.quot) {
      sc_sess->playFile(basedir+int2str(int(num.quot * pow(10.,i)))+suffix, false);
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


CONST_ACTION_2P(SCUSAddAction, ',', false);
EXEC_ACTION_START(SCUSAddAction) {
  string n1 = resolveVars(par1, sess, sc_sess, event_params);
  string n2 = resolveVars(par2, sess, sc_sess, event_params);

  string varname = par1;
  if (varname.length() && varname[0] == '$')
    varname = varname.substr(1);

  // todo: err checking
  string res = double2str(atof(n1.c_str()) + atof(n2.c_str()));

  DBG("setting var[%s] = %s + %s = %s\n", 
      varname.c_str(), n1.c_str(), n2.c_str(), res.c_str());
  sc_sess->var[varname] = res;

} EXEC_ACTION_END;

CONST_ACTION_2P(SCUSSubAction, ',', false);
EXEC_ACTION_START(SCUSSubAction) {
  string n1 = resolveVars(par1, sess, sc_sess, event_params);
  string n2 = resolveVars(par2, sess, sc_sess, event_params);

  string varname = par1;
  if (varname.length() && varname[0] == '$')
    varname = varname.substr(1);

  // todo: err checking
  string res = double2str(atof(n1.c_str()) - atof(n2.c_str()));

  DBG("setting var[%s] = %s - %s = %s\n", 
      varname.c_str(), n1.c_str(), n2.c_str(), res.c_str());
  sc_sess->var[varname] = res;

} EXEC_ACTION_END;

CONST_ACTION_2P(SCUIntAction, ',', false);
EXEC_ACTION_START(SCUIntAction) {
  string val = resolveVars(par2, sess, sc_sess, event_params);

  string varname = par1;
  if (varname.length() && varname[0] == '$')
    varname = varname.substr(1);  

  sc_sess->var[varname] = int2str((int)atof(val.c_str()));
  DBG("set $%s = %s\n", 
      varname.c_str(), sc_sess->var[varname].c_str());

} EXEC_ACTION_END;

CONST_ACTION_2P(SCUSplitStringAction, ',', true);
EXEC_ACTION_START(SCUSplitStringAction) {
  size_t cntr = 0;
  string str = resolveVars(par1, sess, sc_sess, event_params);
  string dst_array = par2;
  if (!dst_array.length())
    dst_array = par1;
  if (dst_array.length() && dst_array[0]=='$')
    dst_array = dst_array.substr(1);
  
  size_t p = 0, last_p = 0;
  while (true) {
    p = str.find("\n", last_p);
    if (p==string::npos) {
      if (last_p < str.length())
	sc_sess->var[dst_array+"["+int2str((unsigned int)cntr)+"]"] = str.substr(last_p);
      break;
    }
    
    sc_sess->var[dst_array+"["+int2str((unsigned int)cntr++)+"]"] = str.substr(last_p, p-last_p);

    last_p = p+1;    
  }
} EXEC_ACTION_END;

EXEC_ACTION_START(SCUEscapeCRLFAction) {
  string varname = arg;
  if (varname.length() && varname[0]=='$')
    varname.erase(0,1);

  while (true) {
    size_t p = sc_sess->var[varname].find("\r\n");
    if (p==string::npos)
      break;
    sc_sess->var[varname].replace(p, 2, "\\r\\n");
  }

  DBG("escaped: $%s='%s'\n", varname.c_str(), sc_sess->var[varname].c_str());
} EXEC_ACTION_END;


EXEC_ACTION_START(SCUUnescapeCRLFAction) {
  string varname = arg;
  if (varname.length() && varname[0]=='$')
    varname.erase(0,1);

  while (true) {
    size_t p = sc_sess->var[varname].find("\\r\\n");
    if (p==string::npos)
      break;
    sc_sess->var[varname].replace(p, 4, "\r\n");
  }

  DBG("unescaped: $%s='%s'\n", varname.c_str(), sc_sess->var[varname].c_str());
} EXEC_ACTION_END;


CONST_ACTION_2P(SCUPlayRingToneAction, ',', true);
EXEC_ACTION_START(SCUPlayRingToneAction) {

  int length = 0;
  int rtparams[4] = {2000, 4000, 440, 480}; // US
  string s_length = resolveVars(par1, sess, sc_sess, event_params);
  if (!str2int(s_length, length)) {
    WARN("could not decipher ringtone length: '%s'\n", s_length.c_str());
  }

  vector<string> r_params = explode(par2, ",");
  for (vector<string>::iterator it=
	 r_params.begin(); it !=r_params.end(); it++) {
    string p = resolveVars(*it, sess, sc_sess, event_params);
    if (p.empty())
      continue;
    if (!str2int(p, rtparams[it-r_params.begin()])) {
      WARN("could not decipher ringtone parameter %zd: '%s', using default\n",
	   it-r_params.begin(), p.c_str());
      continue;
    }
  }

  DBG("Playing ringtone length %d, on %d, off %d, f %d, f2 %d\n",
      length, rtparams[0], rtparams[1], rtparams[2], rtparams[3]);

  DSMRingTone* rt = new DSMRingTone(length, rtparams[0], rtparams[1],
				    rtparams[2], rtparams[3]);
  sc_sess->addToPlaylist(new AmPlaylistItem(rt, NULL));

  sc_sess->transferOwnership(rt);
} EXEC_ACTION_END;
