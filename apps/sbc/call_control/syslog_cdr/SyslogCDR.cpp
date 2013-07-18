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

#include "SBCCallControlAPI.h"

#include <string.h>
#include <syslog.h>

#define CDR_VARS "cdr::v"

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

EXPORT_PLUGIN_CLASS_FACTORY(SyslogCDRFactory,"cc_syslog_cdr");

SyslogCDR* SyslogCDR::_instance=0;

SyslogCDR* SyslogCDR::instance()
{
    if(!_instance)
	_instance = new SyslogCDR();
    return _instance;
}

SyslogCDR::SyslogCDR()
  : level(2), syslog_prefix("CDR: "), quoting_enabled(true)
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

  quoting_enabled = cfg.hasParameter("quoting_enabled") ?
    cfg.getParameter("quoting_enabled") == "yes" : quoting_enabled;

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
    } else if (method == "getExtendedInterfaceHandler") {
      ret.push((AmObject*)this);
    } else if(method == "_list"){
      ret.push("start");
      ret.push("connect");
      ret.push("end");
    }
    else
	throw AmDynInvoke::NotImplemented(method);
}

string timeString(time_t tv_sec) {
  static const string empty;
  if (tv_sec == 0) return empty; // better empty than invalid time

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

  call_profile->cc_vars[CDR_VARS] = values;
}

string getTimeDiffString(int from_ts_sec, int from_ts_usec,
			 int to_ts_sec, int to_ts_usec,
			 bool ms_precision) {
  string res;

  struct timeval start;
  start.tv_sec = from_ts_sec;
  start.tv_usec = from_ts_usec;
  struct timeval diff;
  diff.tv_sec = to_ts_sec;
  diff.tv_usec = to_ts_usec;
  if (!from_ts_sec || !to_ts_sec || timercmp(&start, &diff, >)) {
    diff.tv_sec = diff.tv_usec = 0;
  } else {
    timersub(&diff,&start,&diff);
  }

  if (ms_precision) {
    diff.tv_usec /= 1000;
    string msecs = int2str((unsigned int)diff.tv_usec);
    if (msecs.length()==1)
      msecs = "00"+msecs;
    else if (msecs.length()==2)
      msecs = "0"+msecs;

    res+=int2str((unsigned int)diff.tv_sec)+"."+ msecs;
      
  } else {
    if (diff.tv_usec>=500000)
      diff.tv_sec++;
    res += int2str((unsigned int)diff.tv_sec);
  }
  return res;
}

string do_quote(string s) {
  string res = "\"";
  for (string::iterator it = s.begin();it!=s.end();it++) {
    if (*it == '"') {
      res +="\"\"";
    } else {
      res += *it;
    }
  }
  res += "\"";
  return res;
}

// inlining (?)
#define csv_quote(_str) (quoting_enabled?do_quote(_str) : _str)

void SyslogCDR::end(const string& ltag, SBCCallProfile* call_profile,
		    int start_ts_sec, int start_ts_usec,
		    int connect_ts_sec, int connect_ts_usec,
		    int end_ts_sec, int end_ts_usec) {
  if (!call_profile) return;

  static const int log2syslog_level[] = { LOG_ERR, LOG_WARNING, LOG_INFO,
					  LOG_DEBUG, LOG_NOTICE };

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
  
  SBCVarMapIteratorT vars_it = call_profile->cc_vars.find(CDR_VARS);
  if (vars_it != call_profile->cc_vars.end())
    values = vars_it->second;

  if (cdr_format.size()) {
    for (vector<string>::iterator it=cdr_format.begin(); it != cdr_format.end(); it++) {
      if (it->size() && (*it)[0]=='$') {
	if (*it == "$ltag") {
	  cdr+=csv_quote(ltag) +",";
	} else if (*it == "$start_ts") {
	  cdr+=csv_quote(int2str(start_ts_sec)+"."+int2str(start_ts_usec)) +",";
	} else if (*it == "$connect_ts") {
	  cdr+=csv_quote(int2str(connect_ts_sec)+"."+int2str(connect_ts_usec)) +",";
	} else if (*it == "$end_ts") {
	  cdr+=csv_quote(int2str(end_ts_sec)+"."+int2str(end_ts_usec)) +",";
	} else if (*it == "$duration") {
	  cdr+=csv_quote(getTimeDiffString(start_ts_sec, start_ts_usec,
					   end_ts_sec, end_ts_usec, true)) +",";
	} else if (*it == "$duration_sec") {
	  cdr+=csv_quote(getTimeDiffString(start_ts_sec, start_ts_usec,
					   end_ts_sec, end_ts_usec, false)) +",";
	} else if (*it == "$bill_duration") {
	  cdr+=csv_quote(getTimeDiffString(connect_ts_sec, connect_ts_usec,
					   end_ts_sec, end_ts_usec, true)) +",";
	} else if (*it == "$bill_duration_sec") {
	  cdr+=csv_quote(getTimeDiffString(connect_ts_sec, connect_ts_usec,
					   end_ts_sec, end_ts_usec, false)) +",";
	} else if (*it == "$setup_duration") {
	  if (!connect_ts_sec) {
	    cdr+=csv_quote(getTimeDiffString(start_ts_sec, start_ts_usec,
					     end_ts_sec, end_ts_usec, true)) +",";
	  } else {
	    cdr+=csv_quote(getTimeDiffString(start_ts_sec, start_ts_usec,
					     connect_ts_sec, connect_ts_usec, true)) +",";
	  }
	} else if (*it == "$setup_duration_sec") {
	  if (!connect_ts_sec) {
	    cdr+=csv_quote(getTimeDiffString(start_ts_sec, start_ts_usec,
					     end_ts_sec, end_ts_usec, false)) +",";
	  } else {
	    cdr+=csv_quote(getTimeDiffString(start_ts_sec, start_ts_usec,
					     connect_ts_sec, connect_ts_usec, false)) +",";
	  }
	} else if (*it == "$start_tm") {
	  cdr+=csv_quote(timeString(start_ts_sec)) +",";
	} else if (*it == "$connect_tm") {
	  cdr+=csv_quote(timeString(connect_ts_sec)) +",";
	} else if (*it == "$end_tm") {
	  cdr+=csv_quote(timeString(end_ts_sec)) +",";
	} else {
	  string varname = it->substr(1);
	  string prop;
	  size_t ppos = varname.find('.');
	  if (ppos != string::npos) {
	    prop = varname.substr(ppos+1);
	    varname = varname.substr(0, ppos);
	  }
	  SBCVarMapIteratorT var_it = call_profile->cc_vars.find(varname);
	  if (var_it == call_profile->cc_vars.end()) {
	    DBG("unknown variable '%s' in cdr_format\n", it->c_str());
	  } else {
	    AmArg* v = &var_it->second;
	    if (!prop.empty()) {
	      try {
		v = &var_it->second[prop];
	      }	catch(...) { }
	    }
	    if (isArgCStr((*v))) {
	      cdr+=csv_quote(string(v->asCStr()))+",";
	    } else {
	      cdr+=AmArg::print(*v)+",";
	    }
	  }
	}
      } else {
	if (!values.hasMember(*it)) {
	  cdr+=csv_quote(string("")) + ",";
	} else {
	  if (isArgCStr(values[*it])) {
	    cdr+=csv_quote(string(values[*it].asCStr())) +",";
	  } else {
	    cdr+=csv_quote(AmArg::print(values[*it])) +",";
	  }
	}
      }
    }
  } else {
    // default format: ltag, start_ts, connect_ts, end_ts, <other data...>
    cdr = csv_quote(ltag) + "," +
      csv_quote(int2str(start_ts_sec)+"."+int2str(start_ts_usec)) +","+
      csv_quote(int2str(connect_ts_sec)+"."+int2str(connect_ts_usec)) +","+
      csv_quote(int2str(end_ts_sec)+"."+int2str(end_ts_usec)) +",";
    for (AmArg::ValueStruct::const_iterator i=values.begin(); i!=values.end();i++) {
      if (isArgCStr(i->second)) {
	cdr+=csv_quote(string(i->second.asCStr())) +",";
      } else {
	cdr+=csv_quote(AmArg::print(i->second)) +",";
      }
    }
  }

  if (cdr.size() && cdr[cdr.size()-1]==',')
    cdr.erase(cdr.size()-1, 1);

  syslog(log2syslog_level[level], "%s%s", syslog_prefix.c_str(), cdr.c_str());
  DBG("written CDR '%s' to syslog\n", ltag.c_str());
}

void SyslogCDR::onStateChange(SBCCallLeg *call, const CallLeg::StatusChangeCause &cause)
{
  SBCCallProfile &prof = call->getCallProfile();

  SBCVarMapIteratorT i = prof.cc_vars.find(CDR_VARS);
  if (i == prof.cc_vars.end()) {
    prof.cc_vars[CDR_VARS] = AmArg();
    i = prof.cc_vars.find(CDR_VARS);
  }
  if (i == prof.cc_vars.end()) {
    ERROR("can't update CDR\n");
    return;
  }

  AmArg &cdr = i->second;

  CallLeg::CallStatus s = call->getCallStatus();

  bool answered = false;
  bool have_disposition = cdr.hasMember("disposition");
  if (have_disposition) answered = cdr["disposition"] == "answered";


  // call establishment data (disposition, invite_code, invite_reason)

  if ((s == CallLeg::Connected || s == CallLeg::Disconnected) && !have_disposition) {
    switch (cause.reason) {
      case CallLeg::StatusChangeCause::SipReply:
        // rember reply code/reason if given
        if (!cause.param.reply) {
          ERROR("bug: reply not set when writing to CDR\n");
          return;
        }
        cdr["invite_code"] = (int) cause.param.reply->code;
        cdr["invite_reason"] = cause.param.reply->reason;
        if (cause.param.reply->code < 300)
          cdr["disposition"] = "answered";
        else
          cdr["disposition"] = "failed";
        break;

      case CallLeg::StatusChangeCause::Canceled:
        cdr["disposition"] = "canceled";
        break;

      case CallLeg::StatusChangeCause::NoPrack:
        cdr["disposition"] = "no PRACK";
        break;

      case CallLeg::StatusChangeCause::Other:
          if (s == CallLeg::Connected)
            cdr["disposition"] = "answered";
          else
            cdr["disposition"] = "failed";
          break;

      case CallLeg::StatusChangeCause::NoAck:
      case CallLeg::StatusChangeCause::SipRequest:
      case CallLeg::StatusChangeCause::RtpTimeout:
      case CallLeg::StatusChangeCause::SessionTimeout:
          ERROR("bug: unexpected call state change cause: %d\n", cause.reason);
          cdr["disposition"] = "failed";
          break;

      case CallLeg::StatusChangeCause::InternalError:
          cdr["disposition"] = "failed";
          break;

    }
  }


  // hangup related data (hangup_cause, hangup_initiator), answered calls only!

  if (s == CallLeg::Disconnected && answered) {
    switch (cause.reason) {
      case CallLeg::StatusChangeCause::SipRequest:
        if (cause.param.request) {
          // terminated because of request
          cdr["hangup_cause"] = cause.param.request->method;
          bool our_peer = cause.param.request->from_tag == call->getRemoteTag();
          if ((call->isALeg() && our_peer) || (!call->isALeg() && !our_peer))
            cdr["hangup_initiator"] = "caller";
          else
            cdr["hangup_initiator"] = "callee";
        }
        break;

      case CallLeg::StatusChangeCause::SipReply:
        cdr["hangup_cause"] = "reply";
        break;

      case CallLeg::StatusChangeCause::Canceled:
        // should not get here, the call was not established
        break;

      case CallLeg::StatusChangeCause::NoAck:
        cdr["hangup_cause"] = "no ACK";
        break;

      case CallLeg::StatusChangeCause::NoPrack:
        cdr["hangup_cause"] = "no PRACK";
        break;

      case CallLeg::StatusChangeCause::RtpTimeout:
        cdr["hangup_cause"] = "RTP timeout";
        break;

      case CallLeg::StatusChangeCause::SessionTimeout:
        cdr["hangup_cause"] = "session timeout";
        break;

      case CallLeg::StatusChangeCause::Other:
        if (cause.param.desc) cdr["hangup_cause"] = cause.param.desc;
        else cdr["hangup_cause"] = "other";
        break;

      case CallLeg::StatusChangeCause::InternalError:
          cdr["hangup cause"] = "error";
          cdr["hangup_initiator"] = "local";
          break;
    }

  }
}
