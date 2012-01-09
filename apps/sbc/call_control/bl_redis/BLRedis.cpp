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

#include "BLRedis.h"

#include "ampi/SBCCallControlAPI.h"

#include <string.h>

class CCBLRedisFactory : public AmDynInvokeFactory
{
public:
    CCBLRedisFactory(const string& name)
	: AmDynInvokeFactory(name) {}

    AmDynInvoke* getInstance(){
	return CCBLRedis::instance();
    }

    int onLoad(){
      if (CCBLRedis::instance()->onLoad())
	return -1;

      DBG("REDIS blacklist call control loaded.\n");

      return 0;
    }
};

EXPORT_PLUGIN_CLASS_FACTORY(CCBLRedisFactory, MOD_NAME);

CCBLRedis* CCBLRedis::_instance=0;

CCBLRedis* CCBLRedis::instance()
{
    if(!_instance)
	_instance = new CCBLRedis();
    return _instance;
}

CCBLRedis::CCBLRedis()
{
}

CCBLRedis::~CCBLRedis() { }

int CCBLRedis::onLoad() {
  AmConfigReader cfg;

  string redis_server = "127.0.0.1";
  string redis_port = "6379";
  string redis_reconnect_timers = "5,10,20,50,100,500,1000";
  string redis_connections = "10";
  string redis_max_conn_wait = "1000";

  if(cfg.loadPluginConf(MOD_NAME)) {
    INFO(MOD_NAME "configuration  file not found, assuming default "
	 "configuration is fine\n");
  } else {
    redis_server = cfg.getParameter("redis_server", redis_server);
    redis_port = cfg.getParameter("redis_port", redis_port);
    redis_reconnect_timers =
      cfg.getParameter("redis_reconnect_timers", redis_reconnect_timers);
    redis_connections = cfg.getParameter("redis_connections", redis_connections);
    redis_max_conn_wait = cfg.getParameter("redis_max_conn_wait", redis_max_conn_wait);
  }

  unsigned int i_redis_connections;
  if (str2i(redis_connections, i_redis_connections)) {
    ERROR("could not understand redis_connections=%s\n", redis_connections.c_str());
    return -1;
  }

  unsigned int i_redis_port;
  if (str2i(redis_port, i_redis_port)) {
    ERROR("could not understand redis_port=%s\n", redis_port.c_str());
    return -1;
  }

 unsigned int i_redis_max_conn_wait;
  if (str2i(redis_max_conn_wait, i_redis_max_conn_wait)) {
    ERROR("could not understand redis_max_conn_wait=%s\n", redis_max_conn_wait.c_str());
    return -1;
  }

 std::vector<unsigned int> reconnect_timers;
 std::vector<string> timeouts_v = explode(redis_reconnect_timers, ",");
  for (std::vector<string>::iterator it=
         timeouts_v.begin(); it != timeouts_v.end(); it++) {
    int r;
    if (!str2int(*it, r)) {
      ERROR("REDIS reconnect timeout '%s' not understood\n",
            it->c_str());
      return -1;
    }
    reconnect_timers.push_back(r);
  }

  connection_pool.set_config(redis_server, i_redis_port,
			     reconnect_timers, i_redis_max_conn_wait);
  connection_pool.add_connections(i_redis_connections);
  connection_pool.start();

  return 0;
}

void CCBLRedis::invoke(const string& method, const AmArg& args, AmArg& ret)
{
  DBG("CCBLRedis: %s(%s)\n", method.c_str(), AmArg::print(args).c_str());

  if(method == "start"){
    // INFO("--------------------------------------------------------------\n");
    // INFO("Got call control start ltag '%s' start_ts %i.%i\n",
    // 	   args.get(0).asCStr(), args[2][0].asInt(), args[2][1].asInt());
    // INFO("---- dumping CC values ----\n");
    // for (AmArg::ValueStruct::const_iterator it =
    // 	     args.get(CC_API_PARAMS_CFGVALUES).begin();
    //               it != args.get(CC_API_PARAMS_CFGVALUES).end(); it++) {
    // 	INFO("    CDR value '%s' = '%s'\n", it->first.c_str(), it->second.asCStr());
    // }
    // INFO("--------------------------------------------------------------\n");

    // cc_name, ltag, call profile, timestamps, [[key: val], ...], timer_id
    //args.assertArrayFmt("ssoaui");
    //args[CC_API_PARAMS_TIMESTAMPS].assertArrayFmt("iiiiii");

    SBCCallProfile* call_profile =
      dynamic_cast<SBCCallProfile*>(args[CC_API_PARAMS_CALL_PROFILE].asObject());

    start(args[CC_API_PARAMS_CC_NAMESPACE].asCStr(),
	  args[CC_API_PARAMS_LTAG].asCStr(),
	  call_profile,
	  args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_START_SEC].asInt(),
	  args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_START_USEC].asInt(),
	  args[CC_API_PARAMS_CFGVALUES],
	  args[CC_API_PARAMS_TIMERID].asInt(),  ret);

  } else if(method == "connect"){
    // INFO("--------------------------------------------------------------\n");
    // INFO("Got CDR connect ltag '%s' other_ltag '%s', connect_ts %i.%i\n",
    // 	   args[CC_API_PARAMS_LTAG].asCStr(),
    //           args[CC_API_PARAMS_OTHERID].asCStr(),
    //           args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_CONNECT_SEC].asInt(),
    //           args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_CONNECT_USEC].asInt());
    // INFO("--------------------------------------------------------------\n");
    // cc_name, ltag, call_profile, other_ltag, connect_ts_sec, connect_ts_usec
    // args.assertArrayFmt("ssoas");
    // args[CC_API_PARAMS_TIMESTAMPS].assertArrayFmt("iiiiii");

    SBCCallProfile* call_profile =
      dynamic_cast<SBCCallProfile*>(args[CC_API_PARAMS_CALL_PROFILE].asObject());

    connect(args[CC_API_PARAMS_CC_NAMESPACE].asCStr(),
	    args[CC_API_PARAMS_LTAG].asCStr(),
	    call_profile,
	    args[CC_API_PARAMS_OTHERID].asCStr(),
	    args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_CONNECT_SEC].asInt(),
	    args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_CONNECT_USEC].asInt());

  } else if(method == "end"){
    // INFO("--------------------------------------------------------------\n");
    // INFO("Got CDR end ltag %s end_ts %i.%i\n",
    // 	   args[CC_API_PARAMS_LTAG].asCStr(),
    //           args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_END_SEC].asInt(),
    //           args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_END_USEC].asInt());
    // INFO("--------------------------------------------------------------\n");

    // cc_name, ltag, call_profile, end_ts_sec, end_ts_usec
    // args.assertArrayFmt("ssoa"); 
    // args[CC_API_PARAMS_TIMESTAMPS].assertArrayFmt("iiiiii");

    SBCCallProfile* call_profile =
      dynamic_cast<SBCCallProfile*>(args[CC_API_PARAMS_CALL_PROFILE].asObject());

    end(args[CC_API_PARAMS_CC_NAMESPACE].asCStr(),
	args[CC_API_PARAMS_LTAG].asCStr(),
	call_profile,
	args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_END_SEC].asInt(),
	args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_END_USEC].asInt()
	);
  } else if(method == "_list"){
    ret.push("start");
    ret.push("connect");
    ret.push("end");
  }
  else
    throw AmDynInvoke::NotImplemented(method);
}

void CCBLRedis::start(const string& cc_name, const string& ltag,
		       SBCCallProfile* call_profile,
		       int start_ts_sec, int start_ts_usec,
		       const AmArg& values, int timer_id, AmArg& res) {
  // start code here
  res.push(AmArg());
  AmArg& res_cmd = res[0];

  
  redisContext* c = connection_pool.getActiveConnection();
  if (NULL == c) {
   INFO("no connection to REDIS\n");
    res_cmd[SBC_CC_ACTION] = SBC_CC_REFUSE_ACTION;
    res_cmd[SBC_CC_REFUSE_CODE] = 500;
    res_cmd[SBC_CC_REFUSE_REASON] = "Server Internal Error";  
    return;
  }

  DBG("using redis connection [%p]\n", c);

  connection_pool.returnConnection(c);

  // Drop:
  // res_cmd[SBC_CC_ACTION] = SBC_CC_DROP_ACTION;

  // res_cmd[SBC_CC_ACTION] = SBC_CC_REFUSE_ACTION;
  // res_cmd[SBC_CC_REFUSE_CODE] = 404;
  // res_cmd[SBC_CC_REFUSE_REASON] = "No, not here";  

  // Set Timer:
  // DBG("my timer ID will be %i\n", timer_id);
  // res_cmd[SBC_CC_ACTION] = SBC_CC_SET_CALL_TIMER_ACTION;
  // res_cmd[SBC_CC_TIMER_TIMEOUT] = 5;
}

void CCBLRedis::connect(const string& cc_name, const string& ltag,
			 SBCCallProfile* call_profile,
			 const string& other_tag,
			 int connect_ts_sec, int connect_ts_usec) {
  // connect code here

}

void CCBLRedis::end(const string& cc_name, const string& ltag,
		     SBCCallProfile* call_profile,
		     int end_ts_sec, int end_ts_usec) {
  // end code here

}
