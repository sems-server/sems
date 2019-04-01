/*
 * Copyright (C) 2009 Teltech Systems Inc.
 * Copyright (C) 2015-2016 Juha Heinanen
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
#include "jsonArg.h"

SC_EXPORT(MOD_CLS_NAME);

MOD_ACTIONEXPORT_BEGIN(MOD_CLS_NAME) {

  DEF_CMD("utils.playCountRight", SCUPlayCountRightAction);
  DEF_CMD("utils.playCountLeft",  SCUPlayCountLeftAction);
  DEF_CMD("utils.getCountRight",  SCUGetCountRightAction);
  DEF_CMD("utils.getCountLeft",   SCUGetCountLeftAction);
  DEF_CMD("utils.getCountRightNoSuffix",  SCUGetCountRightNoSuffixAction);
  DEF_CMD("utils.getCountLeftNoSuffix",   SCUGetCountLeftNoSuffixAction);

  DEF_CMD("utils.getNewId", SCGetNewIdAction);
  DEF_CMD("utils.spell", SCUSpellAction);
  DEF_CMD("utils.rand", SCURandomAction);
  DEF_CMD("utils.srand", SCUSRandomAction);
  DEF_CMD("utils.add", SCUSAddAction);
  DEF_CMD("utils.sub", SCUSSubAction);
  DEF_CMD("utils.mul", SCUSMulAction);
  DEF_CMD("utils.int", SCUIntAction);
  DEF_CMD("utils.md5", SCUMD5Action);
  DEF_CMD("utils.replace", SCUReplaceAction);
  DEF_CMD("utils.splitStringCR", SCUSplitStringAction);
  DEF_CMD("utils.splitString", SCUGenSplitStringAction);
  DEF_CMD("utils.decodeJson", SCUDecodeJsonAction);
  DEF_CMD("utils.escapeCRLF", SCUEscapeCRLFAction);
  DEF_CMD("utils.unescapeCRLF", SCUUnescapeCRLFAction);
  DEF_CMD("utils.playRingTone", SCUPlayRingToneAction);

} MOD_ACTIONEXPORT_END;

MOD_CONDITIONEXPORT_BEGIN(MOD_CLS_NAME) {

  if (cmd == "utils.isInList") {
    return new IsInListCondition(params, false);
  }
  if (cmd == "utils.startsWith") {
    return new StartsWithCondition(params, false);
  }

} MOD_CONDITIONEXPORT_END;

vector<string> utils_get_count_files(DSMSession* sc_sess, unsigned int cnt, 
				     const string& basedir, const string& suffix, bool right) {
  
  vector<string> res;

  if (cnt <= 20) {
    res.push_back(basedir+int2str(cnt)+suffix);
    return res;
  }
  
  for (int i=9;i>1;i--) {
    div_t num = div(cnt, (int)pow(10.,i));  
    if (num.quot) {
      res.push_back(basedir+int2str(int(num.quot * pow(10.,i)))+suffix);
    }
    cnt = num.rem;
  }

  if (!cnt)
    return res;

  if ((cnt <= 20) || (!(cnt%10))) {
    res.push_back(basedir+int2str(cnt)+suffix);
    return res;
  }

  div_t num = div(cnt, 10);
  if (right) { 
   // language has single digits before 10s
    res.push_back(basedir+int2str(num.quot * 10)+suffix);
    res.push_back(basedir+("x"+int2str(num.rem))+suffix);    
  } else {
    // language has single digits before 10s
    res.push_back(basedir+("x"+int2str(num.rem))+suffix);    
    res.push_back(basedir+int2str(num.quot * 10)+suffix);
  }

  return res;
}


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

void utils_set_session_vars(DSMSession* sc_sess, string prefix, AmArg json) {
  if (json.getType() == AmArg::Struct) {
    for (AmArg::ValueStruct::const_iterator it1 = json.begin();
	 it1 != json.end(); it1++) {
      utils_set_session_vars(sc_sess, prefix + "." + it1->first, it1->second);
    }
  } else if (json.getType() == AmArg::Array) {
    for (std::vector<AmArg>::size_type i = 0; i != json.size(); i++) {
      utils_set_session_vars(sc_sess, prefix + "[" + int2str((int)i) + "]",
			     json[i]);
    }
  } else {
    string json_string = AmArg::print(json);
    DBG("setting %s = %s\n", prefix.c_str(), json_string.c_str());
    if (isArgCStr(json) && (json_string.size() > 1)) {
      json_string.erase(json_string.end() - 1);
      json_string.erase(json_string.begin());
    }
    sc_sess->var[prefix] = json_string.c_str();      
  }
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


CONST_ACTION_2P(SCUGetCountRightAction, ',', true);
EXEC_ACTION_START(SCUGetCountRightAction) {
  string cnt_s = resolveVars(par1, sess, sc_sess, event_params);
  string basedir = resolveVars(par2, sess, sc_sess, event_params);

  unsigned int cnt = 0;
  if (str2i(cnt_s,cnt)) {
    ERROR("could not parse count '%s'\n", cnt_s.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("could not parse count '"+cnt_s+"'\n");
    return false;
  }

  vector<string> filenames = utils_get_count_files(sc_sess, cnt, basedir, ".wav", true);

  cnt=0;
  for (vector<string>::iterator it=filenames.begin();it!=filenames.end();it++) {
    sc_sess->var["count_file["+int2str(cnt)+"]"]=*it;
    cnt++;
  }

  sc_sess->CLR_ERRNO;
} EXEC_ACTION_END;


CONST_ACTION_2P(SCUGetCountLeftAction, ',', true);
EXEC_ACTION_START(SCUGetCountLeftAction) {
  string cnt_s = resolveVars(par1, sess, sc_sess, event_params);
  string basedir = resolveVars(par2, sess, sc_sess, event_params);

  unsigned int cnt = 0;
  if (str2i(cnt_s,cnt)) {
    ERROR("could not parse count '%s'\n", cnt_s.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("could not parse count '"+cnt_s+"'\n");
    return false;
  }

  vector<string> filenames = utils_get_count_files(sc_sess, cnt, basedir, ".wav", false);

  cnt=0;
  for (vector<string>::iterator it=filenames.begin();it!=filenames.end();it++) {
    sc_sess->var["count_file["+int2str(cnt)+"]"]=*it;
    cnt++;
  }

  sc_sess->CLR_ERRNO;
} EXEC_ACTION_END;


CONST_ACTION_2P(SCUGetCountRightNoSuffixAction, ',', true);
EXEC_ACTION_START(SCUGetCountRightNoSuffixAction) {
  string cnt_s = resolveVars(par1, sess, sc_sess, event_params);
  string basedir = resolveVars(par2, sess, sc_sess, event_params);

  unsigned int cnt = 0;
  if (str2i(cnt_s,cnt)) {
    ERROR("could not parse count '%s'\n", cnt_s.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("could not parse count '"+cnt_s+"'\n");
    return false;
  }

  vector<string> filenames = utils_get_count_files(sc_sess, cnt, basedir, "", true);

  cnt=0;
  for (vector<string>::iterator it=filenames.begin();it!=filenames.end();it++) {
    sc_sess->var["count_file["+int2str(cnt)+"]"]=*it;
    cnt++;
  }

  sc_sess->CLR_ERRNO;
} EXEC_ACTION_END;


CONST_ACTION_2P(SCUGetCountLeftNoSuffixAction, ',', true);
EXEC_ACTION_START(SCUGetCountLeftNoSuffixAction) {
  string cnt_s = resolveVars(par1, sess, sc_sess, event_params);
  string basedir = resolveVars(par2, sess, sc_sess, event_params);

  unsigned int cnt = 0;
  if (str2i(cnt_s,cnt)) {
    ERROR("could not parse count '%s'\n", cnt_s.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("could not parse count '"+cnt_s+"'\n");
    return false;
  }

  vector<string> filenames = utils_get_count_files(sc_sess, cnt, basedir, "", false);

  cnt=0;
  for (vector<string>::iterator it=filenames.begin();it!=filenames.end();it++) {
    sc_sess->var["count_file["+int2str(cnt)+"]"]=*it;
    cnt++;
  }

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

CONST_ACTION_2P(SCUSMulAction, ',', false);
EXEC_ACTION_START(SCUSMulAction) {
  string n1 = resolveVars(par1, sess, sc_sess, event_params);
  string n2 = resolveVars(par2, sess, sc_sess, event_params);

  string varname = par1;
  if (varname.length() && varname[0] == '$')
    varname = varname.substr(1);

  // todo: err checking
  string res = int2str(atoi(n1.c_str()) * atoi(n2.c_str()));

  DBG("setting var[%s] = %s * %s = %s\n", 
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

CONST_ACTION_2P(SCUMD5Action, ',', false);
EXEC_ACTION_START(SCUMD5Action) {
  string n1 = resolveVars(par1, sess, sc_sess, event_params);
  string n2 = resolveVars(par2, sess, sc_sess, event_params);

  string varname = par1;
  if (varname.length() && varname[0] == '$')
    varname = varname.substr(1);

  string res = calculateMD5(n2);

  DBG("setting var[%s] = %s\n", varname.c_str(), res.c_str());
  sc_sess->var[varname] = res;

} EXEC_ACTION_END;

CONST_ACTION_2P(SCUReplaceAction, ',', false);
EXEC_ACTION_START(SCUReplaceAction) {

  string subject = resolveVars(par1, sess, sc_sess, event_params);

  vector<string> vars = explode(par2, "=>");
  if (vars.size() != 2) {
    ERROR("could not parse search=>replace '%s'\n", par2.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("could not parse search=>replace '" + par2 +"'\n");
    return false;
  }

  string search;

  if ((vars[0])[0] != '$') {
    search = vars[0];
  } else {
    search = resolveVars(vars[0], sess, sc_sess, event_params);
    if (search.length() == 0) {
      ERROR("search var '%s' value is empty\n", vars[0].c_str());
      sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
      sc_sess->SET_STRERROR("search var '" + vars[0] + "' value is empty\n");
      return false;
    }
  }

  string replace;

  if ((vars[1])[0] != '$') {
    replace = vars[1];
  } else {
    replace = resolveVars(vars[1], sess, sc_sess, event_params);
  }

  size_t pos = 0;
  while ((pos = subject.find(search, pos)) != std::string::npos) {
    subject.replace(pos, search.length(), replace);
    pos += replace.length();
  }

  string varname = par1;
  if (varname.length() && varname[0] == '$')
    varname = varname.substr(1);

  DBG("setting var[%s] = %s\n", varname.c_str(), subject.c_str());
  sc_sess->var[varname] = subject;

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

CONST_ACTION_2P(SCUGenSplitStringAction, ',', true);
EXEC_ACTION_START(SCUGenSplitStringAction) {
  string str = resolveVars(par1, sess, sc_sess, event_params);
  string delim = resolveVars(par2, sess, sc_sess, event_params);

  string varname = par1;
  if (varname.length() == 0) {
    ERROR("varname is empty\n");
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("varname is empty\n");
    return false;
  }
  if (varname[0] == '$')
    varname = varname.substr(1);

  unsigned int i;
  if (delim.length() == 0) {
    for(i = 0; i < str.size(); ++i) {
      sc_sess->var[varname + "[" + int2str(i) + "]"] = str[i];
    }
  } else {
    size_t p = 0, last_p = 0;
    i = 0;
    while (true) {
      p = str.find(delim, last_p);
      if (p == string::npos) {
	if (last_p <= str.length())
	  sc_sess->var[varname + "[" + int2str(i) + "]"] = str.substr(last_p);
	break;
      }
      sc_sess->var[varname + "[" + int2str(i) + "]"] =
	str.substr(last_p, p - last_p);
      last_p = p + delim.length();
      i++;
    }
  }
} EXEC_ACTION_END;

CONST_ACTION_2P(SCUDecodeJsonAction, ',', true);
EXEC_ACTION_START(SCUDecodeJsonAction) {
  const string json_str = resolveVars(par1, sess, sc_sess, event_params);
  string struct_name = par2;
  if (struct_name.length() == 0) {
    ERROR("struct name is empty\n");
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("struct name is empty\n");
    return false;
  }
  if (struct_name[0] == '$')
    struct_name = struct_name.substr(1);
  AmArg json;
  if (!json2arg(json_str, json)) {
    ERROR("failed to decode json string '%s'\n", json_str.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("failed to decode json string\n");
    return false;
  }
  utils_set_session_vars(sc_sess, struct_name, json);
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

CONST_CONDITION_2P(IsInListCondition, ',', false);
MATCH_CONDITION_START(IsInListCondition) {
  string key = resolveVars(par1, sess, sc_sess, event_params);
  string cslist = resolveVars(par2, sess, sc_sess, event_params);
  DBG("checking whether '%s' is in list '%s'\n", key.c_str(), cslist.c_str());

  bool res = false;
  vector<string> items = explode(cslist, ",");
  for (vector<string>::iterator it=items.begin(); it != items.end(); it++) {
    if (key == trim(*it, " \t")) {
      res = true;
      break;
    }
  }
  DBG("key %sfound\n", res?"":" not");

  if (inv) {
    return !res;
  } else {
    return res;
  }
 } MATCH_CONDITION_END;

CONST_CONDITION_2P(StartsWithCondition, ',', false);
MATCH_CONDITION_START(StartsWithCondition) {

  string key = resolveVars(par1, sess, sc_sess, event_params);
  string prefix = resolveVars(par2, sess, sc_sess, event_params);

  DBG("checking whether '%s' starts with '%s'\n", key.c_str(), prefix.c_str());
  bool res = false;
  res = (key.length() >= prefix.length()) &&
    std::equal(prefix.begin(), prefix.end(), key.begin());
  DBG("prefix %sfound\n", res?"":"not ");

  return res;

} MATCH_CONDITION_END;
