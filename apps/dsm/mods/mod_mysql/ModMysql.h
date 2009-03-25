/*
 * $Id$
 *
 * Copyright (C) 2009 TelTech Systems
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
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
#ifndef _MOD_MYSQL_H
#define _MOD_MYSQL_H
#include "DSMModule.h"
#include "DSMSession.h"

#include <mysql++/mysql++.h>

#define MY_AKEY_CONNECTION "db.con"
#define MY_AKEY_RESULT     "db.res"

#define DSM_ERRNO_MY_CONNECTION "30"
#define DSM_ERRNO_MY_QUERY      "31"
#define DSM_ERRNO_MY_NORESULT   "32"
#define DSM_ERRNO_MY_NOROW      "33"
#define DSM_ERRNO_MY_NOCOLUMN   "34"

class SCMysqlModule 
: public DSMModule {

 public:
  SCMysqlModule();
  ~SCMysqlModule();
  
  DSMAction* getAction(const string& from_str);
  DSMCondition* getCondition(const string& from_str);
};

class DSMMyConnection 
: public mysqlpp::Connection,
  public ArgObject,
  public DSMDisposable 
{
 public:
 DSMMyConnection(const char* db, const char* server, const char* user, const char* password)
   : mysqlpp::Connection(db, server, user, password)
  { }
  ~DSMMyConnection() { }
};

class DSMMyStoreQueryResult 
: public mysqlpp::StoreQueryResult,
  public ArgObject,
  public DSMDisposable 
{
 public:
  DSMMyStoreQueryResult(const StoreQueryResult& other) : 
  mysqlpp::StoreQueryResult(other)
  { }
  ~DSMMyStoreQueryResult() { }
};

DEF_ACTION_1P(SCMyConnectAction);
DEF_ACTION_1P(SCMyDisconnectAction);
DEF_ACTION_1P(SCMyExecuteAction);
DEF_ACTION_1P(SCMyQueryAction);
DEF_ACTION_2P(SCMyQueryGetResultAction);
DEF_ACTION_2P(SCMyGetResultAction);
DEF_ACTION_1P(SCMyGetClientVersion);
DEF_ACTION_1P(SCMyResolveQueryParams);
DEF_SCCondition(MyHasResultCondition);
DEF_SCCondition(MyConnectedCondition);
DEF_ACTION_1P(SCMySaveResultAction);
DEF_ACTION_1P(SCMyUseResultAction);



#endif
