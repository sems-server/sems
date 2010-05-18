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

#include "AmConfigReader.h"
#include "AmEventDispatcher.h"

#include "log.h"

#include <sys/types.h>
#include <regex.h>
#include <unistd.h>

//EXPORT_PLUGIN_CLASS_FACTORY(Monitor, MOD_NAME);
extern "C" void* plugin_class_create()
{
    Monitor* m_inst = Monitor::instance();
    assert(dynamic_cast<AmDynInvokeFactory*>(m_inst));

    return m_inst;
}

Monitor* Monitor::_instance=0;
unsigned int Monitor::gcInterval = 10;

Monitor* Monitor::instance()
{
  if(_instance == NULL)
    _instance = new Monitor(MOD_NAME);
  return _instance;
}

Monitor::Monitor(const string& name) 
  : AmDynInvokeFactory(MOD_NAME), gc_thread(NULL) {
}

Monitor::~Monitor() {
}

int Monitor::onLoad() {
  // todo: if GC configured, start thread
  AmConfigReader cfg;
  if(cfg.loadFile(AmConfig::ModConfigPath + string(MOD_NAME ".conf"))) {
    DBG("monitoring not starting garbage collector\n");
    return 0;
  }

  if (cfg.getParameter("run_garbage_collector","no") == "yes") {
    gcInterval = cfg.getParameterInt("garbage_collector_interval", 10);
    DBG("Running garbage collection for monitoring every %u seconds\n", 
	gcInterval);
    gc_thread.reset(new MonitorGarbageCollector());
    gc_thread->start();
    AmEventDispatcher::instance()->addEventQueue("monitoring_gc", gc_thread.get());
//     // add garbage collector to garbage collector...
//     AmThreadWatcher::instance()->add(gc_thread);
  }

  return 0;
}

void Monitor::invoke(const string& method, 
		     const AmArg& args, AmArg& ret) {
  if((method == "log") || (method == "set")) {
    log(args,ret);
  } else if((method == "logAdd") || (method == "add")) {
    logAdd(args,ret);
  } else if(method == "markFinished"){
    markFinished(args,ret);
  } else if(method == "setExpiration"){
    setExpiration(args,ret);
  } else if(method == "get"){
    get(args,ret);
  } else if(method == "getAttribute"){
    getAttribute(args,ret);
  } else if(method == "getAttributeFinished"){
    getAttributeFinished(args,ret);
  } else if(method == "getAttributeActive"){
    getAttributeActive(args,ret);
  } else if(method == "list"){
    listAll(args,ret);
  } else if(method == "listByFilter"){
    listByFilter(args,ret);
  } else if(method == "listByRegex"){
    listByRegex(args,ret);
  } else if(method == "listFinished"){
    listFinished(args,ret);
  } else if(method == "listActive"){
    listActive(args,ret);
  } else if(method == "clear"){
    clear(args,ret);
  } else if(method == "clearFinished"){
    clearFinished(args,ret);
  } else if(method == "erase"){
    clear(args,ret);
  } else if(method == "_list"){ 
    ret.push(AmArg("log"));
    ret.push(AmArg("set"));
    ret.push(AmArg("logAdd"));
    ret.push(AmArg("add"));
    ret.push(AmArg("markFinished"));
    ret.push(AmArg("setExpiration"));
    ret.push(AmArg("erase"));
    ret.push(AmArg("clear"));
    ret.push(AmArg("clearFinished"));
    ret.push(AmArg("get"));
    ret.push(AmArg("getAttribute"));
    ret.push(AmArg("getAttributeActive"));
    ret.push(AmArg("getAttributeFinished"));
    ret.push(AmArg("list"));
    ret.push(AmArg("listByFilter"));
    ret.push(AmArg("listByRegex"));
    ret.push(AmArg("listFinished"));
    ret.push(AmArg("listActive"));
  } else
    throw AmDynInvoke::NotImplemented(method);
}

void Monitor::log(const AmArg& args, AmArg& ret) {
  assertArgCStr(args[0]);
  
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
    AmArg& val = bucket.log[args[0].asCStr()].info[args[1].asCStr()];
    if (!isArgArray(val)) {
      AmArg v1 = val;
      val = AmArg();
      val.push(v1);
    }
    val.push(AmArg(args[2]));
  } catch (...) {
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
    bucket.log[args[0].asCStr()].finished = time(0);
  bucket.log_lock.unlock();
  ret.push(0);
  ret.push("OK");
}

void Monitor::setExpiration(const AmArg& args, AmArg& ret) {
  assertArgCStr(args[0]);
  assertArgInt(args[1]);

  LogBucket& bucket = getLogBucket(args[0].asCStr());
  bucket.log_lock.lock();
  bucket.log[args[0].asCStr()].finished = args[1].asInt();
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
  clearFinished();

  ret.push(0);
  ret.push("OK");
}

void Monitor::clearFinished() {
  time_t now = time(0);
  for (int i=0;i<NUM_LOG_BUCKETS;i++) {
    logs[i].log_lock.lock();
    std::map<string, LogInfo>::iterator it=
      logs[i].log.begin();
    while (it != logs[i].log.end()) {
      if (it->second.finished && 
	  it->second.finished <= now) {
	std::map<string, LogInfo>::iterator d_it = it;
	it++;
	logs[i].log.erase(d_it);
      } else {
	it++;
      }
    }
    logs[i].log_lock.unlock();
  }
}

void Monitor::get(const AmArg& args, AmArg& ret) {
  assertArgCStr(args[0]);
  ret.assertArray();
  LogBucket& bucket = getLogBucket(args[0].asCStr());
  bucket.log_lock.lock();
  std::map<string, LogInfo>::iterator it=bucket.log.find(args[0].asCStr());
  if (it!=bucket.log.end())
    ret.push(it->second.info);
  bucket.log_lock.unlock();
}

void Monitor::getAttribute(const AmArg& args, AmArg& ret) {
  assertArgCStr(args[0]);
  string attr_name = args[0].asCStr();
  for (int i=0;i<NUM_LOG_BUCKETS;i++) {
    logs[i].log_lock.lock();
    for (std::map<string, LogInfo>::iterator it=
	   logs[i].log.begin();it != logs[i].log.end();it++) {
      ret.push(AmArg());
      AmArg& val = ret.get(ret.size()-1);
      val.push(AmArg(it->first.c_str()));
      val.push(it->second.info[attr_name]);
    }
    logs[i].log_lock.unlock();
  }
}


#define DEF_GET_ATTRIB_FUNC(func_name, cond)				\
  void Monitor::func_name(const AmArg& args, AmArg& ret) {		\
    assertArgCStr(args[0]);						\
    ret.assertArray();							\
    string attr_name = args[0].asCStr();				\
    time_t now = time(0);						\
    for (int i=0;i<NUM_LOG_BUCKETS;i++) {				\
      logs[i].log_lock.lock();						\
      for (std::map<string, LogInfo>::iterator it=			\
	     logs[i].log.begin();it != logs[i].log.end();it++) {	\
	if (cond) {							\
	  ret.push(AmArg());						\
	  AmArg& val = ret.get(ret.size()-1);				\
	  val.push(AmArg(it->first.c_str()));				\
	  val.push(it->second.info[attr_name]);				\
	}								\
      }									\
      logs[i].log_lock.unlock();					\
    }									\
  }

DEF_GET_ATTRIB_FUNC(getAttributeActive,  (!(it->second.finished && 
					    it->second.finished <= now)))
DEF_GET_ATTRIB_FUNC(getAttributeFinished,(it->second.finished && 
					  it->second.finished <= now))
#undef DEF_GET_ATTRIB_FUNC

void Monitor::listAll(const AmArg& args, AmArg& ret) {
  ret.assertArray();
  for (int i=0;i<NUM_LOG_BUCKETS;i++) {
    logs[i].log_lock.lock();
    for (std::map<string, LogInfo>::iterator it=
	   logs[i].log.begin(); it != logs[i].log.end(); it++) {
      ret.push(AmArg(it->first.c_str()));
    }
    logs[i].log_lock.unlock();
  }
}

void Monitor::listByFilter(const AmArg& args, AmArg& ret) {
  ret.assertArray();
  for (int i=0;i<NUM_LOG_BUCKETS;i++) {
    logs[i].log_lock.lock();
    try {
      for (std::map<string, LogInfo>::iterator it=
	     logs[i].log.begin(); it != logs[i].log.end(); it++) {
	bool match = true;
	for (size_t i=0;i<args.size();i++) {
	  AmArg& p = args.get(i);	  
	  if (!(it->second.info[p.get(0).asCStr()]==p.get(1))) {
	    match = false;
	    break;
	  }
	}

	if (!match)
	  continue;

	ret.push(AmArg(it->first.c_str()));  
      }
    } catch(...) {
      logs[i].log_lock.unlock();
      throw;
    }
    logs[i].log_lock.unlock();
  }
}

void Monitor::listByRegex(const AmArg& args, AmArg& ret) {
  assertArgCStr(args[0]);
  assertArgCStr(args[1]);

  ret.assertArray();
  regex_t attr_reg;
  if(regcomp(&attr_reg,args[1].asCStr(),REG_NOSUB)){
    ERROR("could not compile regex '%s'\n", args[1].asCStr());
    return;
  }
  
  for (int i=0;i<NUM_LOG_BUCKETS;i++) {
    logs[i].log_lock.lock();
    try {
      for (std::map<string, LogInfo>::iterator it=
	     logs[i].log.begin(); it != logs[i].log.end(); it++) {
	if (!it->second.info.hasMember(args[0].asCStr())  || 
	    !isArgCStr(it->second.info[args[0].asCStr()]) ||
	    regexec(&attr_reg,it->second.info[args[0].asCStr()].asCStr(),0,0,0))
	  continue;

	ret.push(AmArg(it->first.c_str()));  
      }
    } catch(...) {
      logs[i].log_lock.unlock();
      throw;
    }
    logs[i].log_lock.unlock();
  }

  regfree(&attr_reg);
}

void Monitor::listFinished(const AmArg& args, AmArg& ret) {
  time_t now = time(0);
  ret.assertArray();
  for (int i=0;i<NUM_LOG_BUCKETS;i++) {
    logs[i].log_lock.lock();
    for (std::map<string, LogInfo>::iterator it=
	   logs[i].log.begin(); it != logs[i].log.end(); it++) {
      if (it->second.finished && 
	  it->second.finished <= now)
	ret.push(AmArg(it->first.c_str()));
    }
    logs[i].log_lock.unlock();
  }
}


void Monitor::listActive(const AmArg& args, AmArg& ret) {
  time_t now = time(0);
  ret.assertArray();
  for (int i=0;i<NUM_LOG_BUCKETS;i++) {
    logs[i].log_lock.lock();
    for (std::map<string, LogInfo>::iterator it=
	   logs[i].log.begin(); it != logs[i].log.end(); it++) {
      if (!(it->second.finished &&
	    it->second.finished <= now))
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

void MonitorGarbageCollector::run() {
  DBG("running MonitorGarbageCollector thread\n");
  running.set(true);
  while (running.get()) {
    sleep(Monitor::gcInterval);
    Monitor::instance()->clearFinished();
  }
  DBG("MonitorGarbageCollector thread ends\n");
  AmEventDispatcher::instance()->delEventQueue("monitoring_gc");
}

void MonitorGarbageCollector::postEvent(AmEvent* e) {
  AmSystemEvent* sys_ev = dynamic_cast<AmSystemEvent*>(e);  
  if (sys_ev && 
      sys_ev->sys_event == AmSystemEvent::ServerShutdown) {
    DBG("stopping MonitorGarbageCollector thread\n");
    running.set(false);
    return;
  }

  WARN("received unknown event\n");
}

void MonitorGarbageCollector::on_stop() {
}
