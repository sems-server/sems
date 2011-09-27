/*
 * Copyright (C) 2011 Stefan Sayer
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include "AmPlugIn.h"
#include "log.h"
#include "AmArg.h"

#include "SyslogCDR.h"

#include "ampi/SBCCallControlAPI.h"

#include <string.h>
#include <syslog.h>

class SyslogCDRFactory : public AmDynInvokeFactory
{
public:
    SyslogCDRFactory(const string& name)
	: AmDynInvokeFactory(name) {}

    AmDynInvoke* getInstance(){
	return SyslogCDR::instance();
    }

    int onLoad(){
      DBG(" syslog CSV CDR generation loaded.\n");

      if (SyslogCDR::instance()->onLoad())
	return -1;

      return 0;
    }
};

EXPORT_PLUGIN_CLASS_FACTORY(SyslogCDRFactory,"syslog_cdr");

SyslogCDR* SyslogCDR::_instance=0;

SyslogCDR* SyslogCDR::instance()
{
    if(!_instance)
	_instance = new SyslogCDR();
    return _instance;
}

SyslogCDR::SyslogCDR()
  : level(2), syslog_prefix("CDR: ")
{
}

SyslogCDR::~SyslogCDR() { }

int SyslogCDR::onLoad() {
  AmConfigReader cfg;

  if(cfg.loadFile(AmConfig::ModConfigPath + string(MOD_NAME ".conf"))) {
    INFO(MOD_NAME "configuration  file (%s) not found, "
	 "assuming default configuration is fine\n",
	 (AmConfig::ModConfigPath + string(MOD_NAME ".conf")).c_str());
    return 0;
  }

  syslog_prefix = cfg.hasParameter("cdr_prefix") ? 
    cfg.getParameter("cdr_prefix") : syslog_prefix;

  level = cfg.hasParameter("loglevel") ? 
    cfg.getParameterInt("loglevel") : level;

  if (cfg.hasParameter("cdr_format")) {
    cdr_format = explode(cfg.getParameter("cdr_format"), ",");
  }

  if (level > 3) {
    WARN("log level > 3 not supported\n");
    level = 3;
  }

  return 0;
}

void SyslogCDR::invoke(const string& method, const AmArg& args, AmArg& ret)
{
  // DBG("SyslogCDR: %s(%s)\n", method.c_str(), AmArg::print(args).c_str());

    if(method == "start"){

      // ltag, call profile, start_ts_sec, start_ts_usec, [[key: val], ...], timer_id
      args.assertArrayFmt("soiiui");
      SBCCallProfile* call_profile = dynamic_cast<SBCCallProfile*>(args[1].asObject());

      start(args[0].asCStr(), call_profile, args[2].asInt(), args[3].asInt(), args[4],
	    args[5].asInt(),  ret);

    } else if(method == "connect"){
      // ltag, call_profile, other_ltag, connect_ts_sec, connect_ts_usec
      args.assertArrayFmt("sosii");
      SBCCallProfile* call_profile = dynamic_cast<SBCCallProfile*>(args[1].asObject());

      connect(args.get(0).asCStr(), call_profile, args.get(2).asCStr(),
	      args.get(3).asInt(), args.get(4).asInt());
    } else if(method == "end"){
      // ltag, call_profile, end_ts_sec, end_ts_usec
      args.assertArrayFmt("soii"); 
      SBCCallProfile* call_profile = dynamic_cast<SBCCallProfile*>(args[1].asObject());

      end(args.get(0).asCStr(), call_profile, args.get(2).asInt(), args.get(3).asInt());
    } else if(method == CC_INTERFACE_MAND_VALUES_METHOD){
      // ret.push("Call-ID");
      // ret.push("From-tag");
    } else if(method == "_list"){
      ret.push("start");
      ret.push("connect");
      ret.push("end");
    }
    else
	throw AmDynInvoke::NotImplemented(method);
}

string timeString(time_t tv_sec) {
  char outstr[200];
  struct tm tmp;
  if (!localtime_r(&tv_sec, &tmp) || strftime(outstr, sizeof(outstr), "%F %T", &tmp) == 0) {
    ERROR("converting time\n");
    sprintf(outstr, "<unknown>");
  }
  return string(outstr);
}

void SyslogCDR::start(const string& ltag, SBCCallProfile* call_profile,
		      int start_ts_sec, int start_ts_usec,
		      const AmArg& values, int timer_id, AmArg& res) {
  if (!call_profile) return;
  /*
// #define CHECK_PARAMETER(pname)						\
//   if (!values.hasMember(pname) || !isArgCStr(values[pname]) ||		\
//       !strlen(values[pname].asCStr())) {				\
//     ERROR("configuration error: " pname " missing for SyslogCDR call control!\n"); \
//     res.push(AmArg());							\
//     AmArg& res_cmd = res[0];						\
//     res_cmd[SBC_CC_ACTION] = SBC_CC_REFUSE_ACTION;			\
//     res_cmd[SBC_CC_REFUSE_CODE] = 500;					\
//     res_cmd[SBC_CC_REFUSE_REASON] = SIP_REPLY_SERVER_INTERNAL_ERROR;	\
//     return;								\
//   }
//   CHECK_PARAMETER("Call-ID");
//   CHECK_PARAMETER("From-tag");
// #undef CHECK_PARAMETER
*/

  call_profile->cc_vars["cdr::t::start"] = start_ts_sec;
  call_profile->cc_vars["cdr::t::start_us"] = start_ts_usec;
  call_profile->cc_vars["cdr::v"] = values;
}


void SyslogCDR::connect(const string& ltag, SBCCallProfile* call_profile,
			const string& other_tag,
			int connect_ts_sec, int connect_ts_usec) {
  if (!call_profile) return;

  call_profile->cc_vars["cdr::t::connect"] = connect_ts_sec;
  call_profile->cc_vars["cdr::t::connect_us"] = connect_ts_usec;
}


void SyslogCDR::end(const string& ltag, SBCCallProfile* call_profile,
		    int end_ts_sec, int end_ts_usec) {
  if (!call_profile) return;

  int start_ts_sec=0, start_ts_usec=0, connect_ts_sec=0, connect_ts_usec=0;

  static const int log2syslog_level[] = { LOG_ERR, LOG_WARNING, LOG_INFO, LOG_DEBUG };

  // for (SBCVarMapIteratorT vars_it = call_profile->cc_vars.begin(); vars_it != call_profile->cc_vars.end(); vars_it++) {
  //   ERROR ("cc_vars '%s' = '%s'\n", vars_it->first.c_str(), AmArg::print(vars_it->second).c_str());
  // }


#define GET_INT_VAR(vname, vdst)					\
  {									\
    SBCVarMapIteratorT vars_it = call_profile->cc_vars.find(vname);	\
    if (vars_it != call_profile->cc_vars.end() && isArgInt(vars_it->second)) { \
      vdst = vars_it->second.asInt();					\
    }									\
  }

  GET_INT_VAR("cdr::t::start", start_ts_sec);
  GET_INT_VAR("cdr::t::start_us", start_ts_usec);
  GET_INT_VAR("cdr::t::connect", connect_ts_sec);
  GET_INT_VAR("cdr::t::connect_us", connect_ts_usec);
#undef GET_INT_VAR

  struct timeval start;
  start.tv_sec = connect_ts_sec;
  start.tv_usec = connect_ts_usec;
  struct timeval diff;
  diff.tv_sec = end_ts_sec;
  diff.tv_usec = end_ts_usec;
  if (!connect_ts_sec || timercmp(&start, &diff, >)) {
    diff.tv_sec = diff.tv_usec = 0;
  } else {
    timersub(&diff,&start,&diff);
  }

  string cdr;

  AmArg d;
  AmArg& values = d;
  
  SBCVarMapIteratorT vars_it = call_profile->cc_vars.find("cdr::v");
  if (vars_it != call_profile->cc_vars.end())
    values = vars_it->second;

  if (cdr_format.size()) {
    for (vector<string>::iterator it=cdr_format.begin(); it != cdr_format.end(); it++) {
      if (it->size() && (*it)[0]=='$') {
	if (*it == "$ltag") {
	  cdr+=ltag+",";
	} else if (*it == "$start_ts") {
	  cdr+=int2str(start_ts_sec)+"."+int2str(start_ts_usec)+",";
	} else if (*it == "$connect_ts") {
	  cdr+=int2str(connect_ts_sec)+"."+int2str(connect_ts_usec)+",";
	} else if (*it == "$end_ts") {
	  cdr+=int2str(end_ts_sec)+"."+int2str(end_ts_usec)+",";
	} else if (*it == "$duration") {
	  cdr+=int2str((unsigned int)diff.tv_sec)+"."+
	    int2str((unsigned int)diff.tv_usec)+",";
	} else if (*it == "$start_tm") {
	  cdr+=timeString(start_ts_sec)+",";
	} else if (*it == "$connect_tm") {
	  cdr+=timeString(connect_ts_sec)+",";
	} else if (*it == "$end_tm") {
	  cdr+=timeString(end_ts_sec)+",";
	} else {
	  ERROR("in configuration: unknown value '%s' in cdr_format\n",
		it->c_str());
	}
      } else {
	if (!values.hasMember(*it)) {
	    cdr+=",";
	} else {
	  if (isArgCStr(values[*it])) {
	    cdr+=string(values[*it].asCStr())+",";
	  } else {
	    cdr+=AmArg::print(values[*it])+",";
	  }
	}
      }
    }
  } else {
    // default format: ltag, start_ts, connect_ts, end_ts, <other data...>
    cdr = ltag + "," +
      int2str(start_ts_sec)+"."+int2str(start_ts_usec)+","+
      int2str(connect_ts_sec)+"."+int2str(connect_ts_usec)+","+
      int2str(end_ts_sec)+"."+int2str(end_ts_usec)+",";
    for (AmArg::ValueStruct::const_iterator i=values.begin(); i!=values.end();i++) {
      if (isArgCStr(i->second)) {
	cdr+=string(i->second.asCStr())+",";
      } else {
	cdr+=AmArg::print(i->second)+",";
      }
    }
  }

  if (cdr.size() && cdr[cdr.size()-1]==',')
    cdr.erase(cdr.size()-1, 1);

  syslog(log2syslog_level[level], "%s%s", syslog_prefix.c_str(), cdr.c_str());
  DBG("written CDR '%s' to syslog\n", ltag.c_str());
}
