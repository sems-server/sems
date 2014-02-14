/*
 * Copyright (C) 2012 Stefan Sayer
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

#ifndef _CC_BL_REDIS_H
#define _CC_BL_REDIS_H

#include "AmApi.h"
#include "RedisConnectionPool.h"

#include "hiredis/hiredis.h"

#include "SBCCallProfile.h"


#define CMD_PASS           0
#define CMD_DROP           1
#define CMD_REFUSE         2

#define RWT_E_OK           0
#define RWT_E_CONNECTION  -1
#define RWT_E_WRITE       -2

/**
 * REDIS blacklist query call control module
 */
class CCBLRedis : public AmDynInvoke
{
  static CCBLRedis* _instance;

  bool   pass_on_bl_unavailable;
  unsigned int max_retries;

  bool full_logging;
  int handle_redis_reply(redisContext* redis_context, redisReply* reply, bool& hit);

  void start(const string& cc_name, const string& ltag, SBCCallProfile* call_profile,
	     int start_ts_sec, int start_ts_usec, const AmArg& values,
	     int timer_id, AmArg& res);
  void connect(const string& cc_name, const string& ltag, SBCCallProfile* call_profile,
	       const string& other_ltag,
	       int connect_ts_sec, int connect_ts_usec);
  void end(const string& cc_name, const string& ltag, SBCCallProfile* call_profile,
	   int end_ts_sec, int end_ts_usec);

  RedisConnectionPool connection_pool;

 public:
  CCBLRedis();
  ~CCBLRedis();
  static CCBLRedis* instance();
  void invoke(const string& method, const AmArg& args, AmArg& ret);
  int onLoad();
};

#endif 
