/*
 * Copyright (C) 2014 Stefan Sayer
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

#include "ModRedis.h"
#include "log.h"
#include "AmUtils.h"

#include "DSMSession.h"
#include "AmSession.h"
#include "AmPlaylist.h"
#include "DSMCoreModule.h"

#include <stdio.h>
#include <fstream>

SC_EXPORT(DSMRedisModule);

DSMRedisModule::DSMRedisModule() {
}

DSMRedisModule::~DSMRedisModule() {
}


DSMAction* DSMRedisModule::getAction(const string& from_str) {
  string cmd;
  string params;
  splitCmd(from_str, cmd, params);

  DEF_CMD("redis.connect",            DSMRedisConnectAction);
  DEF_CMD("redis.disconnect",         DSMRedisDisconnectAction);
  DEF_CMD("redis.execCommand",        DSMRedisExecCommandAction);
  DEF_CMD("redis.appendCommand",      DSMRedisAppendCommandAction);
  DEF_CMD("redis.getReply",           DSMRedisGetReplyAction);

  return NULL;
}

DSMCondition* DSMRedisModule::getCondition(const string& from_str) {
  // string cmd;
  // string params;
  // splitCmd(from_str, cmd, params);

  // if (cmd == "redis.hasResult") {
  //   return new MyHasResultCondition(params, false);
  // }

  // if (cmd == "redis.connected") {
  //   return new MyConnectedCondition(params, true);
  // }

  return NULL;
}

DSMRedisResult::~DSMRedisResult() {
  if (NULL != result) {
    freeReplyObject(result);
  }
}

void DSMRedisResult::release() {
  result = NULL;
}

DSMRedisConnection* getRedisDSMSessionConnection(DSMSession* sc_sess) {
  if (sc_sess->avar.find(REDIS_AKEY_CONNECTION) == sc_sess->avar.end()) {
    SET_ERROR(sc_sess, DSM_ERRNO_REDIS_CONNECTION, "No connection to redis database");
    return NULL;
  }
  AmObject* ao = NULL; DSMRedisConnection* res = NULL;
  try {
    if (!isArgAObject(sc_sess->avar[REDIS_AKEY_CONNECTION])) {
      SET_ERROR(sc_sess, DSM_ERRNO_REDIS_CONNECTION, "No connection to redis database (not AmObject)");
      return NULL;
    }
    ao = sc_sess->avar[REDIS_AKEY_CONNECTION].asObject();
  } catch (...){
    SET_ERROR(sc_sess, DSM_ERRNO_REDIS_CONNECTION, "No connection to redis database (not AmObject)");
    return NULL;
  }

  if (NULL == ao || NULL == (res = dynamic_cast<DSMRedisConnection*>(ao))) {
    SET_ERROR(sc_sess, DSM_ERRNO_REDIS_CONNECTION, "No connection to database (not a RedisConnection)");
    return NULL;
  }

  return res;
}

DSMRedisConnection* getConnectedRedisDSMSessionConnection(DSMSession* sc_sess) {
  DSMRedisConnection* res = getRedisDSMSessionConnection(sc_sess);
  if (!res || res->connected() || res->connect())
    return res;

  return NULL;
}


DSMRedisResult* getRedisDSMResult(DSMSession* sc_sess) {
  if (sc_sess->avar.find(REDIS_AKEY_RESULT) == sc_sess->avar.end()) {
    SET_ERROR(sc_sess, DSM_ERRNO_REDIS_NORESULT, "No result available");
    return NULL;
  }
  AmObject* ao = NULL; DSMRedisResult* res = NULL;
  try {
    assertArgAObject(sc_sess->avar[REDIS_AKEY_RESULT]);
    ao = sc_sess->avar[REDIS_AKEY_RESULT].asObject();
  } catch (...){
    SET_ERROR(sc_sess, DSM_ERRNO_REDIS_NORESULT, "Result object has wrong type");
    return NULL;
  }

  if (NULL == ao || NULL == (res = dynamic_cast<DSMRedisResult*>(ao))) {
    SET_ERROR(sc_sess, DSM_ERRNO_REDIS_NORESULT, "Result object has wrong type");
    return NULL;
  }
  return res;
}

string replaceQueryParams(const string& q, DSMSession* sc_sess, 
			  map<string,string>* event_params) {
  string res = q;
  size_t repl_pos = 0;
  while (repl_pos<res.length()) {
    size_t rstart = res.find_first_of("$#", repl_pos);
    repl_pos = rstart+1;
    if (rstart == string::npos) 
      break;
    if (rstart && res[rstart-1] == '\\') // escaped
      continue;
    
    size_t rend = res.find_first_of(" ,()$#\t;'\"", rstart+1);
    if (rend==string::npos)
      rend = res.length();
    switch(res[rstart]) {
    case '$': 
      res.replace(rstart, rend-rstart, 
		  sc_sess->var[res.substr(rstart+1, rend-rstart-1)]); break;
    case '#':
      if (NULL!=event_params) {
	res.replace(rstart, rend-rstart, 
		    (*event_params)[res.substr(rstart+1, rend-rstart-1)]); break;
      }
    default: break;
    }
  }
  return res;
}

string skip_till(string& s, string sep) {
  size_t pos = s.find_first_of(sep);
  if (pos == string::npos) {
    string res = s;
    s.clear();
    return res;
  } else {
    string res = s.substr(0, pos);
    if (s.length()>pos)
      s = s.substr(pos+1);
    else
      s.clear();
    return res;
  }
}

EXEC_ACTION_START(DSMRedisConnectAction) {
  string f_arg = resolveVars(arg, sess, sc_sess, event_params);
  string db_url = f_arg.length()?f_arg:sc_sess->var["config.redis_db_url"];
  if (db_url.empty() || db_url.length() < 11 || db_url.substr(0, 8) != "redis://") {
    ERROR("missing correct db_url config or connect parameter - must start with redis://\n");
    SET_ERROR(sc_sess, DSM_ERRNO_UNKNOWN_ARG,
	      "missing correct db_url config or connect parameter - must start with redis://\n");
    EXEC_ACTION_STOP;
  }

  db_url = db_url.substr(8);

  // split url - tcp:host;param=value or unix:host;param=value

  string db_proto = skip_till(db_url, ":");
  if (db_proto != "unix" && db_proto != "tcp") {
    ERROR("missing correct redis_db_url config or connect parameter - must start with unix or tcp protocol\n");
    SET_ERROR(sc_sess, DSM_ERRNO_UNKNOWN_ARG,
	      "missing correct db_url config or connect parameter - must start with unix or tcp protocol\n");
    EXEC_ACTION_STOP;
  }
  bool unix_socket = db_proto=="unix";

  string db_host = skip_till(db_url, ":;");
  if (db_host.empty()) {
    ERROR("missing correct redis_db_url config or connect parameter - host must be non-empty\n");
    SET_ERROR(sc_sess, DSM_ERRNO_UNKNOWN_ARG,
	      "missing correct db_url config or connect parameter - host must be non-empty\n");
    EXEC_ACTION_STOP;
  }

  unsigned int redis_port = DEFAULT_REDIS_PORT;
  bool full_logging = false; 
  bool use_transactions = false;
  int connect_timeout = DEFAULT_REDIS_CONNECT_TIMEOUT;

  while (!db_url.empty()) {
    string param = skip_till(db_url, ";");
    vector<string> p = explode(param, "=");
    if (p.size() != 2) {
      ERROR("missing correct redis_db_url config or connect parameter - "
	    "parameter '%s' must be param=value\n", param.c_str());
      SET_ERROR(sc_sess, DSM_ERRNO_UNKNOWN_ARG,
		"missing correct db_url config or connect parameter - parameter must be param=value\n");
      EXEC_ACTION_STOP;
    }

    if (p[0] == "full_logging") {
      full_logging = p[1] == "true";
    } else if (p[0] == "use_transactions") {
      use_transactions = p[1] == "true";
    } else if (p[0] == "connect_timeout"){
      str2int(p[1], connect_timeout);
    } else if (p[0] == "port"){
      str2i(p[1], redis_port);
    } else {
      ERROR("unknown redis_db_url config or connect parameter - "
	    "parameter '%s'\n", p[0].c_str());
      SET_ERROR(sc_sess, DSM_ERRNO_UNKNOWN_ARG, "missing correct db_url config or connect parameter\n");
      EXEC_ACTION_STOP;
    }
  }

  DSMRedisConnection* conn =
    new DSMRedisConnection(db_host, redis_port, unix_socket, full_logging, 
			   use_transactions, connect_timeout);

  if (!conn->connect()) {
    delete conn;
    ERROR("Could not connect to redis DB\n");
    SET_ERROR(sc_sess, DSM_ERRNO_REDIS_CONNECTION, "Could not connect to redis DB");
    EXEC_ACTION_STOP;
  }

  // save connection for later use
  AmArg c_arg;
  c_arg.setBorrowedPointer(conn);
  sc_sess->avar[REDIS_AKEY_CONNECTION] = c_arg;
  // for garbage collection
  sc_sess->transferOwnership(conn);
  CLR_ERROR(sc_sess);    

} EXEC_ACTION_END;

EXEC_ACTION_START(DSMRedisDisconnectAction) {
  DSMRedisConnection* conn = getRedisDSMSessionConnection(sc_sess);
  if (NULL == conn) {
    EXEC_ACTION_STOP;
  }

  conn->disconnect();
  // connection object might be reused - but its safer to create a new one
  sc_sess->releaseOwnership(conn);
  delete conn;
  sc_sess->avar.erase(REDIS_AKEY_CONNECTION);
  sc_sess->CLR_ERRNO;
} EXEC_ACTION_END;

void decodeRedisResult(VarMapT& dst, const string& varname, redisReply* reply) {
  if (!reply)
    return;
  switch (reply->type) {
  case REDIS_REPLY_STRING: dst[varname] = string(reply->str, reply->len); break;
  case REDIS_REPLY_INTEGER: dst[varname] = int2str((int)reply->integer); break; // todo: long long?
  case REDIS_REPLY_NIL: dst[varname] = "nil"; break;
  case REDIS_REPLY_STATUS: dst[varname] = string(reply->str, reply->len); break;
  case REDIS_REPLY_ERROR: ERROR("decoding REDIS reply - ERROR type"); break;
  case REDIS_REPLY_ARRAY: {
    for (size_t i=0;i<reply->elements;i++) {
      decodeRedisResult(dst, varname+"["+int2str((unsigned int)i)+"]", reply->element[i]); 
    }
  } break;
  }
}

void handleResult(DSMSession* sc_sess, int res, redisReply* reply, const string& resultvar) {
  switch (res) {
  case DB_E_OK: {
    decodeRedisResult(sc_sess->var, resultvar, reply);
    freeReplyObject(reply);
  } break;
  case DB_E_CONNECTION: SET_ERROR(sc_sess, DSM_ERRNO_REDIS_CONNECTION, "REDIS connection error"); return;
  case DB_E_WRITE:      SET_ERROR(sc_sess, DSM_ERRNO_REDIS_WRITE,      "REDIS write error"); return;
  case DB_E_READ:       SET_ERROR(sc_sess, DSM_ERRNO_REDIS_READ,       "REDIS read error"); return;
  default:              SET_ERROR(sc_sess, DSM_ERRNO_REDIS_UNKNOWN,    "REDIS unknown error"); return;
  }
}

CONST_ACTION_2P(DSMRedisExecCommandAction, '=', false);
EXEC_ACTION_START(DSMRedisExecCommandAction) {
  string resultvar = par1;
  if (resultvar.length() && resultvar[0]=='$') resultvar=resultvar.substr(1);
  string cmd = replaceParams(par2, sess, sc_sess, event_params);
  DBG("executing redis command $%s='%s'\n", resultvar.c_str(), cmd.c_str());

  DSMRedisConnection* conn = getConnectedRedisDSMSessionConnection(sc_sess);
  if (!conn) {
    SET_ERROR(sc_sess, DSM_ERRNO_REDIS_CONNECTION, "Not connected to REDIS\n");
    EXEC_ACTION_STOP;
  }

  redisReply* reply;
  int res = conn->exec_cmd(cmd.c_str(), reply);

  handleResult(sc_sess, res, reply, resultvar);
} EXEC_ACTION_END;

EXEC_ACTION_START(DSMRedisAppendCommandAction) {
  string cmd = replaceParams(arg, sess, sc_sess, event_params);
  DBG("appending redis command '%s' - from '%s'\n", cmd.c_str(), arg.c_str());

  DSMRedisConnection* conn = getConnectedRedisDSMSessionConnection(sc_sess);
  if (!conn) {
    SET_ERROR(sc_sess, DSM_ERRNO_REDIS_CONNECTION, "Not connected to REDIS\n");
    EXEC_ACTION_STOP;
  }

  if (conn->append_cmd(cmd.c_str()) == DB_E_OK) {
    CLR_ERROR(sc_sess);
  } else {
    SET_ERROR(sc_sess, DSM_ERRNO_REDIS_CONNECTION, "Error appending command - no memory?\n");
  }
} EXEC_ACTION_END;

EXEC_ACTION_START(DSMRedisGetReplyAction) {
  string resultvar = arg;
  if (resultvar.length() && resultvar[0]=='$') resultvar=resultvar.substr(1);
  DBG("getting result for redis command in $%s\n", resultvar.c_str());

  DSMRedisConnection* conn = getConnectedRedisDSMSessionConnection(sc_sess);
  if (!conn) {
    SET_ERROR(sc_sess, DSM_ERRNO_REDIS_CONNECTION, "Not connected to REDIS\n");
    EXEC_ACTION_STOP;
  }

  redisReply* reply;
  int res = conn->get_reply(reply);

  handleResult(sc_sess, res, reply, resultvar);
} EXEC_ACTION_END;
