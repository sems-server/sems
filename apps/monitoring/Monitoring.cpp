/*
 * $Id$
 *
 * Copyright (C) 2009 IPTEGO GmbH
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
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

#include "Monitoring.h"

#include "log.h"

//EXPORT_PLUGIN_CLASS_FACTORY(Monitor, MOD_NAME);
extern "C" void* plugin_class_create()
{
    Monitor* m_inst = Monitor::instance();
    assert(dynamic_cast<AmDynInvokeFactory*>(m_inst));

    return m_inst;
}

Monitor* Monitor::_instance=0;

Monitor* Monitor::instance()
{
  if(_instance == NULL)
    _instance = new Monitor(MOD_NAME);
  return _instance;
}

Monitor::Monitor(const string& name) 
  : AmDynInvokeFactory(MOD_NAME) {
}

Monitor::~Monitor() {
}

int Monitor::onLoad() {
  // todo: if GC configured, start thread
  return 0;
}

void Monitor::invoke(const string& method, 
		     const AmArg& args, AmArg& ret) {
  if(method == "log"){
    log(args,ret);
  } else if(method == "logAdd"){
    logAdd(args,ret);
  } else if(method == "markFinished"){
    markFinished(args,ret);
  } else if(method == "clear"){
    clear(args,ret);
  } else if(method == "clearFinished"){
    clearFinished(args,ret);
  } else if(method == "erase"){
    clear(args,ret);
  } else if(method == "get"){
    get(args,ret);
  } else if(method == "list"){
    list(args,ret);
  } else if(method == "listFinished"){
    listFinished(args,ret);
  } else if(method == "listUnfinished"){
    listUnfinished(args,ret);
  } else if(method == "_list"){ 
    ret.push(AmArg("log"));
    ret.push(AmArg("logAdd"));
    ret.push(AmArg("markFinished"));
    ret.push(AmArg("erase"));
    ret.push(AmArg("clear"));
    ret.push(AmArg("clearFinished"));
    ret.push(AmArg("get"));
    ret.push(AmArg("list"));
    ret.push(AmArg("listFinished"));
    ret.push(AmArg("listUnfinished"));
  } else
    throw AmDynInvoke::NotImplemented(method);
}

void Monitor::log(const AmArg& args, AmArg& ret) {
  assertArgCStr(args[0]);
  assertArgCStr(args[1]);
  
  LogBucket& bucket = getLogBucket(args[0].asCStr());
  bucket.log_lock.lock();
  try {
    for (size_t i=1;i<args.size();i+=2)
    bucket.log[args[0].asCStr()].info[args[i].asCStr()]=AmArg(args[i+1]);
  } catch (...) {
    bucket.log_lock.unlock();
    ret.push(-1);
    ret.push("ERROR while converting value");
    throw;
  }
  bucket.log_lock.unlock();
  ret.push(0);
  ret.push("OK");
}

void Monitor::logAdd(const AmArg& args, AmArg& ret) {
  assertArgCStr(args[0]);
  assertArgCStr(args[1]);

  LogBucket& bucket = getLogBucket(args[0].asCStr());
  bucket.log_lock.lock();
  try {
    bucket.log[args[0].asCStr()].info[args[1].asCStr()].push(AmArg(args[2]));
  } catch (...) {
    ret.push(-1);
    ret.push("ERROR while converting value");
    bucket.log_lock.unlock();
    throw;
  }
  ret.push(0);
  ret.push("OK");
  bucket.log_lock.unlock();
}

void Monitor::markFinished(const AmArg& args, AmArg& ret) {
  assertArgCStr(args[0]);

  LogBucket& bucket = getLogBucket(args[0].asCStr());
  bucket.log_lock.lock();
  if (!bucket.log[args[0].asCStr()].finished)
    bucket.log[args[0].asCStr()].finished  = time(0);
  bucket.log_lock.unlock();
  ret.push(0);
  ret.push("OK");
}

void Monitor::erase(const AmArg& args, AmArg& ret) {
  assertArgCStr(args[0]);
  LogBucket& bucket = getLogBucket(args[0].asCStr());
  bucket.log_lock.lock();
  bucket.log.erase(args[0].asCStr());
  bucket.log_lock.unlock();
  ret.push(0);
  ret.push("OK");
}

void Monitor::clear(const AmArg& args, AmArg& ret) {
  for (int i=0;i<NUM_LOG_BUCKETS;i++) {
    logs[i].log_lock.lock();
    logs[i].log.clear();
    logs[i].log_lock.unlock();
  }
  ret.push(0);
  ret.push("OK");
}

void Monitor::clearFinished(const AmArg& args, AmArg& ret) {
 for (int i=0;i<NUM_LOG_BUCKETS;i++) {
    logs[i].log_lock.lock();
    std::map<string, LogInfo>::iterator it=
      logs[i].log.begin();
    while (it != logs[i].log.end()) {
      if (it->second.finished > 0) {
	std::map<string, LogInfo>::iterator d_it = it;
	it++;
	logs[i].log.erase(d_it);
      } else {
	it++;
      }
    }
    logs[i].log_lock.unlock();
  }
  ret.push(0);
  ret.push("OK");
}

void Monitor::get(const AmArg& args, AmArg& ret) {
  assertArgCStr(args[0]);
  LogBucket& bucket = getLogBucket(args[0].asCStr());
  bucket.log_lock.lock();
  std::map<string, LogInfo>::iterator it=bucket.log.find(args[0].asCStr());
  if (it!=bucket.log.end())
    ret.push(it->second.info);
  bucket.log_lock.unlock();
}

void Monitor::list(const AmArg& args, AmArg& ret) {
 for (int i=0;i<NUM_LOG_BUCKETS;i++) {
    logs[i].log_lock.lock();
    for (std::map<string, LogInfo>::iterator it=
	   logs[i].log.begin(); it != logs[i].log.end(); it++) {
      ret.push(AmArg(it->first.c_str()));
    }
    logs[i].log_lock.unlock();
  }
}

void Monitor::listFinished(const AmArg& args, AmArg& ret) {
 for (int i=0;i<NUM_LOG_BUCKETS;i++) {
    logs[i].log_lock.lock();
    for (std::map<string, LogInfo>::iterator it=
	   logs[i].log.begin(); it != logs[i].log.end(); it++) {
      if (it->second.finished > 0)
	ret.push(AmArg(it->first.c_str()));
    }
    logs[i].log_lock.unlock();
  }
}


void Monitor::listUnfinished(const AmArg& args, AmArg& ret) {
 for (int i=0;i<NUM_LOG_BUCKETS;i++) {
    logs[i].log_lock.lock();
    for (std::map<string, LogInfo>::iterator it=
	   logs[i].log.begin(); it != logs[i].log.end(); it++) {
      if (!it->second.finished)
	ret.push(AmArg(it->first.c_str()));
    }
    logs[i].log_lock.unlock();
  }
}

LogBucket& Monitor::getLogBucket(const string& call_id) {
  if (call_id.empty())
    return logs[0];
  char c = '\0'; // some distribution...bad luck if all callid start with 00000...
  for (size_t i=0;i<5 && i<call_id.length();i++) 
    c = c ^ call_id[i];
  
  return logs[c % NUM_LOG_BUCKETS];
}
