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

  if (level > 4) {
    WARN("log level > 4 not supported\n");
    level = 4;
  }

  return 0;
}

void SyslogCDR::invoke(const string& method, const AmArg& args, AmArg& ret)
{
  // DBG("SyslogCDR: %s(%s)\n", method.c_str(), AmArg::print(args).c_str());

    if(method == "start"){
      SBCCallProfile* call_profile =
	dynamic_cast<SBCCallProfile*>(args[CC_API_PARAMS_CALL_PROFILE].asObject());

      start(args[CC_API_PARAMS_LTAG].asCStr(),
	    call_profile, args[CC_API_PARAMS_CFGVALUES]);

    } else if(method == "connect"){
      // no action needed
    } else if(method == "end"){
      SBCCallProfile* call_profile =
	dynamic_cast<SBCCallProfile*>(args[CC_API_PARAMS_CALL_PROFILE].asObject());

      end(args[CC_API_PARAMS_LTAG].asCStr(),
	  call_profile,
	  args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_START_SEC].asInt(),
	  args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_START_USEC].asInt(),
	  args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_CONNECT_SEC].asInt(),
	  args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_CONNECT_USEC].asInt(),
	  args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_END_SEC].asInt(),
	  args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_END_USEC].asInt()
	  );
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
		      const AmArg& values) {
  if (!call_profile) return;

  call_profile->cc_vars["cdr::v"] = values;
}

void SyslogCDR::end(const string& ltag, SBCCallProfile* call_profile,
		    int start_ts_sec, int start_ts_usec,
		    int connect_ts_sec, int connect_ts_usec,
		    int end_ts_sec, int end_ts_usec) {
  if (!call_profile) return;

  static const int log2syslog_level[] = { LOG_ERR, LOG_WARNING, LOG_INFO, LOG_DEBUG, LOG_NOTICE };

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
