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

EXPORT_PLUGIN_CLASS_FACTORY(SyslogCDRFactory,"cdr");

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
      args.assertArrayFmt("sssiu"); // ltag, callid, from_tag, start_ts, [[key: val], ...]

      // INFO("--------------------------------------------------------------\n");
      // INFO("Got CDR start ltag '%s' callid '%s', from_tag '%s', start_ts %i\n",
      // 	   args.get(0).asCStr(), args.get(1).asCStr(), args.get(2).asCStr(),
      // 	   args.get(3).asInt());
      // INFO("---- dumping CDR values ----\n");
      // for (AmArg::ValueStruct::const_iterator it =
      // 	     args.get(4).begin(); it != args.get(4).end(); it++) {
      // 	INFO("    CDR value '%s' = '%s'\n", it->first.c_str(), it->second.asCStr());
      // }
      // INFO("---- dumping CDR values ----\n");
      // INFO("--------------------------------------------------------------\n");

      start(args.get(0).asCStr(), args.get(1).asCStr(), args.get(2).asCStr(),
	    args.get(3).asInt(), args.get(4));

    } else if(method == "connect"){
      // INFO("--------------------------------------------------------------\n");
      // INFO("Got CDR connect ltag '%s' callid '%s', to_tag '%s', connect_ts %i\n",
      // 	   args.get(0).asCStr(), args.get(1).asCStr(), args.get(2).asCStr(),
      // 	   args.get(3).asInt());
      // INFO("--------------------------------------------------------------\n");
      connect(args.get(0).asCStr(), args.get(1).asCStr(), args.get(2).asCStr(),
	      args.get(3).asInt());
    } else if(method == "end"){
      // INFO("--------------------------------------------------------------\n");
      // INFO("Got CDR end ltag '%s' callid '%s', end_ts %i\n",
      // 	   args.get(0).asCStr(), args.get(1).asCStr(), args.get(2).asInt());
      // INFO("--------------------------------------------------------------\n");
      end(args.get(0).asCStr(), args.get(1).asCStr(), args.get(2).asInt());
    } else if(method == "_list"){
      ret.push("start");
      ret.push("connect");
      ret.push("end");
    }
    else
	throw AmDynInvoke::NotImplemented(method);
}

void SyslogCDR::start(const string& ltag, const string& callid, const string& from_tag,
		   int start_ts, AmArg& values) {
  cdrs_mut.lock();
  CDR* cdr = new CDR();
  cdr->ltag = ltag;
  cdr->callid = callid;
  cdr->from_tag = from_tag;
  cdr->start_ts = start_ts;

  for (AmArg::ValueStruct::const_iterator it =
	 values.begin(); it != values.end(); it++) {
    cdr->values[it->first] = it->second.asCStr();
  }


  map<string, CDR*>::iterator it=cdrs.find(ltag);
  if (it != cdrs.end())
    delete it->second; // double entry?

  cdrs[ltag] = cdr;

  cdrs_mut.unlock();
}

void SyslogCDR::connect(const string& ltag, const string& callid, const string& to_tag,
		     int connect_ts) {
  cdrs_mut.lock();
  map<string, CDR*>::iterator it=cdrs.find(ltag);
  if (it == cdrs.end()) {
    ERROR("CDR with ltag '%s' for connect() not found\n", ltag.c_str());
    cdrs_mut.unlock();
    return;
  }

  it->second->to_tag = to_tag;
  it->second->connect_ts = connect_ts;

  cdrs_mut.unlock();
}

void SyslogCDR::end(const string& ltag, const string& callid, int end_ts) {
  CDR* cdr = NULL;
  static const int log2syslog_level[] = { LOG_ERR, LOG_WARNING, LOG_INFO, LOG_DEBUG };

  cdrs_mut.lock();
  map<string, CDR*>::iterator it=cdrs.find(ltag);
  if (it == cdrs.end()) {
    cdrs_mut.unlock();
    ERROR("CDR with ltag '%s' for end() not found\n", ltag.c_str());
    return;
  }

  cdr = it->second;

  cdrs.erase(it);
  cdrs_mut.unlock();

  cdr->end_ts = end_ts;
  syslog(log2syslog_level[level], "%s%s", syslog_prefix.c_str(), cdr->print_csv().c_str());
  DBG("written CDR '%s' to syslog\n", ltag.c_str());
  delete cdr;

}

string CDR::print_csv() {
  string res;
  res = ltag + "," + callid + "," + from_tag + "," + to_tag + "," +
    int2str(start_ts) + "," + int2str(connect_ts) + "," + int2str(end_ts);
  for (std::map<string, string>::iterator it=
	 values.begin(); it != values.end(); it++) {
    res+=","+it->second;
  }
  return res;
}

string CDR::print_headers() {
  string res;
  res = "Local Tag" ","  "Call-ID"  "," "From Tag"  ","  "To Tag"  ","
    "start TS"  "," "connect TS" "," "end TS";

  for (std::map<string, string>::iterator it=
	 values.begin(); it != values.end(); it++) {
    res+=","+it->first;
  }
  return res;
}
