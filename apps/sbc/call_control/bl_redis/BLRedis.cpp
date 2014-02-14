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

#include "SBCCallControlAPI.h"
#include "AmSipHeaders.h"

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
  pass_on_bl_unavailable = false;

  full_logging = false;

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
    full_logging = cfg.getParameter("redis_full_logging", "no")=="yes";

    pass_on_bl_unavailable = cfg.getParameter("pass_on_bl_unavailable", "no")=="yes";
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

  DBG("setting max number max_retries to %u (as connections)\n", i_redis_connections);
  max_retries = i_redis_connections;

  return 0;
}

void CCBLRedis::invoke(const string& method, const AmArg& args, AmArg& ret)
{
  DBG("CCBLRedis: %s(%s)\n", method.c_str(), AmArg::print(args).c_str());

  if(method == "start"){
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

    // SBCCallProfile* call_profile =
    //   dynamic_cast<SBCCallProfile*>(args[CC_API_PARAMS_CALL_PROFILE].asObject());

    // connect(args[CC_API_PARAMS_CC_NAMESPACE].asCStr(),
    // 	    args[CC_API_PARAMS_LTAG].asCStr(),
    // 	    call_profile,
    // 	    args[CC_API_PARAMS_OTHERID].asCStr(),
    // 	    args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_CONNECT_SEC].asInt(),
    // 	    args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_CONNECT_USEC].asInt());

  } else if(method == "end"){

    // SBCCallProfile* call_profile =
    //   dynamic_cast<SBCCallProfile*>(args[CC_API_PARAMS_CALL_PROFILE].asObject());

    // end(args[CC_API_PARAMS_CC_NAMESPACE].asCStr(),
    // 	args[CC_API_PARAMS_LTAG].asCStr(),
    // 	call_profile,
    // 	args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_END_SEC].asInt(),
    // 	args[CC_API_PARAMS_TIMESTAMPS][CC_API_TS_END_USEC].asInt()
    // 	);
  } else if(method == "_list"){
    ret.push("start");
    ret.push("connect");
    ret.push("end");
  }
  else
    throw AmDynInvoke::NotImplemented(method);
}


int CCBLRedis::handle_redis_reply(redisContext* redis_context, redisReply* reply, bool& hit) {

  hit = false;

  if (!reply)  {
    switch (redis_context->err) {
    case REDIS_ERR_IO:
      ERROR("I/O error: %s\n", strerror(errno));
      return RWT_E_CONNECTION;

    case REDIS_ERR_EOF: // silently reconnect
      return RWT_E_CONNECTION;

    case REDIS_ERR_PROTOCOL:
      ERROR("REDIS Protocol error detected\n");
      return RWT_E_CONNECTION;
    case REDIS_ERR_OTHER:
      return RWT_E_CONNECTION;
    }
  }

  switch (reply->type) {
  case REDIS_REPLY_ERROR:
    ERROR("REDIS ERROR: %s\n", reply->str);
    return RWT_E_WRITE;

  case REDIS_REPLY_STATUS:
  case REDIS_REPLY_STRING:
    if (reply->len>=0) {
      if (full_logging) {
	DBG("REDIS: %.*s\n", reply->len, reply->str);
      }
      hit = true;
    } break;

  case REDIS_REPLY_INTEGER:
    if (full_logging) {
      DBG("REDIS: %lld\n", reply->integer);
    }
    // TODO: add other return codes/cmd
    if (reply->integer) {
      hit = true;
    } break;

  case REDIS_REPLY_ARRAY: {
    for (size_t i=0;i<reply->elements;i++) {
      switch(reply->element[i]->type) {
      case REDIS_REPLY_ERROR: ERROR("REDIS ERROR: %.*s\n", reply->element[i]->len,
				    reply->element[i]->str);
	return RWT_E_WRITE;

      case REDIS_REPLY_INTEGER:
	if (full_logging) {
	  DBG("REDIS: %lld\n", reply->element[i]->integer);
	} 
	if (reply->element[i]->integer) {
	  hit = true;
	}
	break;

      case REDIS_REPLY_NIL: 
	if (full_logging) {
	  DBG("REDIS: nil\n");
	} break;

      case REDIS_REPLY_STATUS:
      case REDIS_REPLY_STRING:
	if (full_logging) {
	  if (reply->element[i]->len >= 0) {
	    DBG("REDIS: %.*s\n", reply->element[i]->len, reply->element[i]->str); 
	  }
	}
	if (reply->element[i]->len >= 0) {
	  hit = true;
	}
	break;
      default:
	ERROR("unknown REDIS reply %d!",reply->element[i]->type); break;
      }
    }
  }; break;

  default: ERROR("unknown REDIS reply %d!", reply->type); break;
  }

  return RWT_E_OK;
}


void CCBLRedis::start(const string& cc_name, const string& ltag,
		       SBCCallProfile* call_profile,
		       int start_ts_sec, int start_ts_usec,
		       const AmArg& values, int timer_id, AmArg& res) {
  // start code here
  res.push(AmArg());
  AmArg& res_cmd = res[0];

#define MAX_ARGV_ITEMS    20
  const char* argv[MAX_ARGV_ITEMS];
  size_t argvlen[MAX_ARGV_ITEMS];
  
  unsigned int argv_max = 0;

  if (!values.hasMember("argc") ||
      str2i(values["argc"].asCStr(), argv_max) || (!argv_max)) {
    ERROR("deciphering argc\n");
    res_cmd[SBC_CC_ACTION] = SBC_CC_REFUSE_ACTION;
    res_cmd[SBC_CC_REFUSE_CODE] = 500;
    res_cmd[SBC_CC_REFUSE_REASON] = SIP_REPLY_SERVER_INTERNAL_ERROR;
    return;
  }

  unsigned int argv_index=0;
  string query;
  for (; argv_index<argv_max;argv_index++) {
    argv[argv_index] = values["argv_"+int2str(argv_index)].asCStr();
    argvlen[argv_index] = strlen(argv[argv_index]);
    if (query.length())
      query+=" ";
    query+=string(argv[argv_index], argvlen[argv_index]);
  }

  DBG("query to REDIS: '%s'\n", query.c_str());

  bool hit = false;

  unsigned int retries = 0;
  for (;retries<max_retries;retries++) {
    redisContext* redis_context = connection_pool.getActiveConnection();
    if (NULL == redis_context) {
      INFO("no connection to REDIS\n");
      if (!pass_on_bl_unavailable) {
	res_cmd[SBC_CC_ACTION] = SBC_CC_REFUSE_ACTION;
	res_cmd[SBC_CC_REFUSE_CODE] = 500;
	res_cmd[SBC_CC_REFUSE_REASON] = SIP_REPLY_SERVER_INTERNAL_ERROR;
      }
      return;
    }

    DBG("using redis connection [%p]\n", redis_context);

    redisReply* reply = (redisReply *)
      redisCommandArgv(redis_context, argv_index, argv, argvlen);

    int ret = handle_redis_reply(redis_context, reply, hit);

    if (ret == RWT_E_CONNECTION) {
      WARN("connection [%p] failed - retrying\n", redis_context);
      connection_pool.returnFailedConnection(redis_context);
      continue;
    }

    connection_pool.returnConnection(redis_context);
    break;
  }

  if (retries == max_retries) {
    if (!pass_on_bl_unavailable) {
      res_cmd[SBC_CC_ACTION] = SBC_CC_REFUSE_ACTION;
      res_cmd[SBC_CC_REFUSE_CODE] = 500;
      res_cmd[SBC_CC_REFUSE_REASON] = SIP_REPLY_SERVER_INTERNAL_ERROR;
    }
    return;
  }


  if (hit) {
    if (values.hasMember("action") && isArgCStr(values["action"]) && 
	values["action"] == "drop") {
      DBG("Blacklist: Dropping call\n");
      res_cmd[SBC_CC_ACTION] = SBC_CC_DROP_ACTION;
    } else {
      DBG("Blacklist: Refusing call\n");
      res_cmd[SBC_CC_ACTION] = SBC_CC_REFUSE_ACTION;
      res_cmd[SBC_CC_REFUSE_CODE] = 403;
      res_cmd[SBC_CC_REFUSE_REASON] = "Unauthorized";  
    }
  }
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
