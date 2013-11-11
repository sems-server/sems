/*
 * Copyright (C) 2009 IPTEGO GmbH
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
unsigned int Monitor::retain_samples_s = 10;

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

  retain_samples_s = cfg.getParameterInt("retain_samples_s", 10);

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
  } else if(method == "getSingle"){
    getSingle(args,ret);
  } else if(method == "inc"){
    inc(args,ret);
  } else if(method == "dec"){
    dec(args,ret);
  } else if(method == "addCount"){
    addCount(args,ret);
  } else if(method == "addSample"){
    addSample(args,ret);
  } else if(method == "getCount"){
    getCount(args,ret);
  } else if(method == "getAllCounts"){
      getAllCounts(args,ret);
  } else if(method == "getAttribute"){
    getAttribute(args,ret);
  } else if(method == "getAttributeFinished"){
    getAttributeFinished(args,ret);
  } else if(method == "getAttributeActive"){
    getAttributeActive(args,ret);
  } else if(method == "list"){
    listAll(args,ret);
  } else if(method == "listByFilter"){
    listByFilter(args,ret, false);
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
  } else if(method == "eraseByFilter"){
    listByFilter(args,ret, true);
  } else if(method == "_list"){ 
    ret.push(AmArg("log"));
    ret.push(AmArg("set"));
    ret.push(AmArg("logAdd"));
    ret.push(AmArg("add"));
    ret.push(AmArg("inc"));
    ret.push(AmArg("dec"));
    ret.push(AmArg("addSample"));
    ret.push(AmArg("markFinished"));
    ret.push(AmArg("setExpiration"));
    ret.push(AmArg("erase"));
    ret.push(AmArg("eraseByFilter"));
    ret.push(AmArg("clear"));
    ret.push(AmArg("clearFinished"));
    ret.push(AmArg("get"));
    ret.push(AmArg("getAttribute"));
    ret.push(AmArg("getAttributeActive"));
    ret.push(AmArg("getAttributeFinished"));
    ret.push(AmArg("getCount"));
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

void Monitor::add(const AmArg& args, AmArg& ret, int a) {
  assertArgCStr(args[0]);

  LogBucket& bucket = getLogBucket(args[0].asCStr());
  bucket.log_lock.lock();
  try {
    //for (size_t i=1;i<args.size();i++) {
    int val = 0;
    AmArg& v = bucket.log[args[0].asCStr()].info[args[1].asCStr()];
    if (isArgInt(v))
      val = v.asInt();
    val+=a;
    v = val;
    //}
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

void Monitor::inc(const AmArg& args, AmArg& ret) {
  add(args, ret, 1);
}

void Monitor::dec(const AmArg& args, AmArg& ret) {
  add(args, ret, -1);
}

void Monitor::addCount(const AmArg& args, AmArg& ret) {
  assertArgInt(args[2]);
  add(args, ret, args[2].asInt());
}

void Monitor::logAdd(const AmArg& args, AmArg& ret) {
  assertArgCStr(args[0]);
  assertArgCStr(args[1]);

  LogBucket& bucket = getLogBucket(args[0].asCStr());
  bucket.log_lock.lock();
  try {
    AmArg& val = bucket.log[args[0].asCStr()].info[args[1].asCStr()];
    if (!isArgArray(val) && !isArgUndef(val)) {
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


// Expected args:
// name, key, [counter=1], [timestamp=now]
void Monitor::addSample(const AmArg& args, AmArg& ret) {
  assertArgCStr(args[0]);
  assertArgCStr(args[1]);

  struct timeval now;
  int cnt = 1;

  if (args.size() > 2 && isArgInt(args[2])) {
    cnt = args[2].asInt();

    if (args.size() > 3 && isArgBlob(args[3])) {
      now = *((struct timeval*) args[3].asBlob()->data);
    }
    else {
      gettimeofday(&now, NULL);
    }
  }
  else if (args.size() > 2 && isArgBlob(args[2])) {
    now = *((struct timeval*)args[2].asBlob()->data);
  } else {
    gettimeofday(&now, NULL);
  }

  LogBucket& bucket = getLogBucket(args[0].asCStr());
  bucket.log_lock.lock();
  list<SampleInfo::time_cnt>& sample_list
    = bucket.samples[args[0].asCStr()].sample[args[1].asCStr()];
  if ((!sample_list.empty()) && timercmp(&sample_list.front().time, &now, >=)) {
    // sample list time stamps needs to be monotonically increasing - clear if resyncing
    // WARN("clock drift backwards - clearing %zd items\n", sample_list.size());
    sample_list.clear();
  }
  sample_list.push_front(SampleInfo::time_cnt(now, cnt));
  bucket.log_lock.unlock();

  ret.push(0);
  ret.push("OK");
}

void Monitor::truncate_samples(
            list<SampleInfo::time_cnt>& v, struct timeval now) {
  struct timeval cliff = now;
  cliff.tv_sec -= retain_samples_s;
  while ((!v.empty()) && timercmp(&cliff, &(v.back().time), >=))
    v.pop_back();
}


// Expected args:
// name, key, [now [from [to]]]   (blob type)
//  or:
// name, key, interval in sec     (int type)
void Monitor::getCount(const AmArg& args, AmArg& ret) {
  assertArgCStr(args[0]);
  assertArgCStr(args[1]);

  struct timeval now;
  if (args.size()>2 && isArgBlob(args[2])) {
    now = *(struct timeval*)args[2].asBlob()->data;
  } else {
    gettimeofday(&now, NULL);
  }

  struct timeval from;
  struct timeval to;
  if (args.size()>3 && isArgBlob(args[3])) {
    from = *(struct timeval*)args[3].asBlob()->data;

    if (args.size()>4 && isArgBlob(args[4]))
      to = *(struct timeval*)args[4].asBlob()->data;
    else
      to = now;

  } else {
    from = to = now;
    if (args.size()>2 && isArgInt(args[2])) {
      from.tv_sec -= args[2].asInt();
    } else {
      from.tv_sec -=1; // default: last second
    }
  }

  if (!now.tv_sec) {
    gettimeofday(&to, NULL);
  }

  unsigned int res = 0;

  LogBucket& bucket = getLogBucket(args[0].asCStr());
  bucket.log_lock.lock();

  map<string, SampleInfo>::iterator it =
        bucket.samples.find(args[0].asCStr());
  if (it != bucket.samples.end()) {

    map<string, list<SampleInfo::time_cnt> >::iterator s_it =
          it->second.sample.find(args[1].asCStr());
    if (s_it != it->second.sample.end()) {

      list<SampleInfo::time_cnt>& v = s_it->second;
      truncate_samples(v, now);
      // todo (?): erase empty sample list
      // if (v.empty()) {
      // 	// sample vector is empty
      // 	it->second.sample.erase(s_it);
      // } else {
      list<SampleInfo::time_cnt>::iterator v_it = v.begin();

      while (v_it != v.end() && timercmp(&(v_it->time), &to, >))
	v_it++;
      if (v_it != v.end()) {
	while (timercmp(&(v_it->time), &from, >=) && v_it != v.end()) {
	  res += v_it->counter;
	  v_it++;
	}
      }

    }
  }
  bucket.log_lock.unlock();

  ret.push((int)res);
}

// Expected args:
// name, [now [from [to]]]   (blob type)
//  or:
// name, interval in sec [now]  (int type)
void Monitor::getAllCounts(const AmArg& args, AmArg& ret) {
  assertArgCStr(args[0]);
	ret.assertStruct();

  struct timeval now;
  if (args.size()>1 && isArgBlob(args[1])) {
    now = *(struct timeval*)args[1].asBlob()->data;
  } else if (args.size()>2 && isArgInt(args[1]) && isArgBlob(args[2])) {
    now = *(struct timeval*)args[2].asBlob()->data;
	} else {
    gettimeofday(&now, NULL);
  }

  struct timeval from;
  struct timeval to;
  if (args.size()>2 && isArgBlob(args[1]) && isArgBlob(args[2])) {
    from = *(struct timeval*)args[2].asBlob()->data;

    if (args.size()>3 && isArgBlob(args[3]))
      to = *(struct timeval*)args[3].asBlob()->data;
    else
      to = now;

  } else {
    from = to = now;
    if (args.size()>1 && isArgInt(args[1])) {
      from.tv_sec -= args[1].asInt();
    } else {
      from.tv_sec -=1; // default: last second
    }
  }

  if (!now.tv_sec) {
    gettimeofday(&to, NULL);
  }

  LogBucket& bucket = getLogBucket(args[0].asCStr());
  bucket.log_lock.lock();

  map<string, SampleInfo>::iterator it =
        bucket.samples.find(args[0].asCStr());
  if (it != bucket.samples.end()) {

    for (map<string, list<SampleInfo::time_cnt> >::iterator s_it =
          it->second.sample.begin(); s_it != it->second.sample.end() ; s_it++) {

      list<SampleInfo::time_cnt>& v = s_it->second;
      truncate_samples(v, now);
      list<SampleInfo::time_cnt>::iterator v_it = v.begin();

      unsigned int res = 0;

      while (timercmp(&(v_it->time), &to, >) && v_it != v.end())
        v_it++;
      if (v_it != v.end()) {
        while (timercmp(&(v_it->time), &from, >=) && v_it != v.end()) {
          res += v_it->counter;
          v_it++;
        }
      }

      ret[s_it->first] = (int)res;
    }

  }
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
  bucket.samples.erase(args[0].asCStr());
  bucket.log_lock.unlock();
  ret.push(0);
  ret.push("OK");
}

void Monitor::clear(const AmArg& args, AmArg& ret) {
  for (int i=0;i<NUM_LOG_BUCKETS;i++) {
    logs[i].log_lock.lock();
    logs[i].log.clear();
    logs[i].samples.clear();
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
	logs[i].samples.erase(d_it->first);
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

void Monitor::getSingle(const AmArg& args, AmArg& ret) {
  assertArgCStr(args[0]);
  assertArgCStr(args[1]);
  ret.assertArray();

  DBG("getSingle(%s,%s)",
      args[0].asCStr(),
      args[1].asCStr());

  LogBucket& bucket = getLogBucket(args[0].asCStr());
  bucket.log_lock.lock();
  std::map<string, LogInfo>::iterator it=bucket.log.find(args[0].asCStr());
  if (it!=bucket.log.end()){
    AmArg& _v = it->second.info;
    DBG("found log: %s",AmArg::print(_v).c_str());
    if(isArgStruct(_v) && _v.hasMember(args[1].asCStr())) {
      ret.push(_v[args[1].asCStr()]);
    }
  }
  bucket.log_lock.unlock();
  DBG("ret = %s",AmArg::print(ret).c_str());
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

void Monitor::listByFilter(const AmArg& args, AmArg& ret, bool erase) {
  ret.assertArray();
  for (int i=0;i<NUM_LOG_BUCKETS;i++) {
    logs[i].log_lock.lock();
    try {
      std::map<string, LogInfo>::iterator it=logs[i].log.begin();

      while (it != logs[i].log.end()) {
	bool match = true;
	for (size_t a_i=0;a_i<args.size();a_i++) {
	  AmArg& p = args.get(a_i);	  
	  if (!(it->second.info[p.get(0).asCStr()]==p.get(1))) {
	    match = false;
	    break;
	  }
	}

	if (match) {
	  ret.push(AmArg(it->first.c_str()));
	  if (erase) {
	    std::map<string, LogInfo>::iterator d_it=it;
	    it++;
	    logs[i].log.erase(d_it);
	    continue;
	  }
	}
	it++;
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
