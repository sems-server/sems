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
#ifndef _MOD_REDIS_H
#define _MOD_REDIS_H
#include "DSMModule.h"
#include "DSMSession.h"

#include "DRedisConnection.h"

#define REDIS_AKEY_CONNECTION "db_redis.con"
#define REDIS_AKEY_RESULT     "db_redis.res"

#define DSM_ERRNO_REDIS_CONNECTION "connection"
#define DSM_ERRNO_REDIS_WRITE      "write"
#define DSM_ERRNO_REDIS_READ       "read"
#define DSM_ERRNO_REDIS_QUERY      "query"
#define DSM_ERRNO_REDIS_NORESULT   "result"
#define DSM_ERRNO_REDIS_NOROW      "result"
#define DSM_ERRNO_REDIS_NOCOLUMN   "result"
#define DSM_ERRNO_REDIS_UNKNOWN    "unknown"

class DSMRedisModule 
: public DSMModule {

 public:
  DSMRedisModule();
  ~DSMRedisModule();
  
  DSMAction* getAction(const string& from_str);
  DSMCondition* getCondition(const string& from_str);
};

class DSMRedisConnection
: public DRedisConnection,
  public AmObject,
  public DSMDisposable 
{
 public:
 DSMRedisConnection(const string& host, unsigned int port,
		    bool unix_socket, bool full_logging, bool use_transactions, int connect_timeout)
   : DRedisConnection(DRedisConfig(host, port, unix_socket, full_logging, use_transactions, connect_timeout))
  { }
  ~DSMRedisConnection() { }
};

class DSMRedisResult
: public AmObject,
  public DSMDisposable 
{
  redisReply* result;
  
 public:
 DSMRedisResult(redisReply* result) : result(result) { }
  ~DSMRedisResult();
  void release();
 };

DEF_ACTION_1P(DSMRedisConnectAction);
DEF_ACTION_1P(DSMRedisDisconnectAction);
DEF_ACTION_2P(DSMRedisExecCommandAction);
DEF_ACTION_1P(DSMRedisAppendCommandAction);
DEF_ACTION_1P(DSMRedisGetReplyAction);
#endif
