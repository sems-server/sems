/*
 * Copyright (C) 2009 TelTech Systems
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
#ifndef _MOD_MYSQL_H
#define _MOD_MYSQL_H
#include "DSMModule.h"
#include "DSMSession.h"

#include <cppconn/sqlstring.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <mysql_connection.h>

#define MY_AKEY_DRIVER     "db.dri"
#define MY_AKEY_CONNECTION "db.con"
#define MY_AKEY_RESULT     "db.res"

#define DSM_ERRNO_MY_DRIVER     "driver"
#define DSM_ERRNO_MY_CONNECTION "connection"
#define DSM_ERRNO_MY_QUERY      "query"
#define DSM_ERRNO_MY_NORESULT   "result"
#define DSM_ERRNO_MY_NOROW      "result"
#define DSM_ERRNO_MY_NOCOLUMN   "result"
#define DSM_ERRNO_MY_NODATA     "result"

class SCMysqlModule 
: public DSMModule {

 public:
  SCMysqlModule();
  ~SCMysqlModule();
  
  DSMAction* getAction(const string& from_str);
  DSMCondition* getCondition(const string& from_str);

};

class DSMMyConnection 
: public AmObject,
  public DSMDisposable 
{
 public:
 DSMMyConnection() : con(NULL)
  {
  }
  ~DSMMyConnection() {
    if (con) {
	    if (!con->isClosed()) con->close();
      delete con;
    }
  }
  sql::Connection* con;
};

class DSMMyStoreQueryResult 
: public AmObject,
  public DSMDisposable 
{
  public:
  DSMMyStoreQueryResult() : res(NULL)
    {
    }
  ~DSMMyStoreQueryResult() {
    if (res) delete res;
  }
  sql::ResultSet* res;
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
DEF_ACTION_2P(SCMyPlayDBAudioAction);
DEF_ACTION_2P(SCMyPlayDBAudioFrontAction);
DEF_ACTION_2P(SCMyPlayDBAudioLoopedAction);
DEF_ACTION_2P(SCMyGetFileFromDBAction);
DEF_ACTION_2P(SCMyPutFileToDBAction);
DEF_ACTION_2P(SCMyEscapeAction);

#endif
