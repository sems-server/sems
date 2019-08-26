/*
 * Copyright (C) 2009 TelTech Systems Inc.
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

#include "ModMysql.h"
#include "log.h"
#include "AmUtils.h"

#include "DSMSession.h"
#include "AmSession.h"
#include "AmPlaylist.h"

#include <stdio.h>
#include <fstream>
#include <unistd.h>

SC_EXPORT(SCMysqlModule);

SCMysqlModule::SCMysqlModule() {
}

SCMysqlModule::~SCMysqlModule() {
}


DSMAction* SCMysqlModule::getAction(const string& from_str) {
  string cmd;
  string params;
  splitCmd(from_str, cmd, params);

  DEF_CMD("mysql.connect",            SCMyConnectAction);
  DEF_CMD("mysql.disconnect",         SCMyDisconnectAction);
  DEF_CMD("mysql.execute",            SCMyExecuteAction);
  DEF_CMD("mysql.query",              SCMyQueryAction);
  DEF_CMD("mysql.queryGetResult",     SCMyQueryGetResultAction);
  DEF_CMD("mysql.getResult",          SCMyGetResultAction);
  DEF_CMD("mysql.getClientVersion",   SCMyGetClientVersion);
  DEF_CMD("mysql.resolveQueryParams", SCMyResolveQueryParams);
  DEF_CMD("mysql.saveResult",         SCMySaveResultAction);
  DEF_CMD("mysql.useResult",          SCMyUseResultAction);
  DEF_CMD("mysql.playDBAudio",        SCMyPlayDBAudioAction);
  DEF_CMD("mysql.playDBAudioFront",   SCMyPlayDBAudioFrontAction);
  DEF_CMD("mysql.playDBAudioLooped",  SCMyPlayDBAudioLoopedAction);
  DEF_CMD("mysql.getFileFromDB",      SCMyGetFileFromDBAction);
  DEF_CMD("mysql.putFileToDB",        SCMyPutFileToDBAction);
  DEF_CMD("mysql.escape",             SCMyEscapeAction);
  return NULL;
}

DSMCondition* SCMysqlModule::getCondition(const string& from_str) {
  string cmd;
  string params;
  splitCmd(from_str, cmd, params);

  if (cmd == "mysql.hasResult") {
    return new MyHasResultCondition(params, false);
  }

  if (cmd == "mysql.connected") {
    return new MyConnectedCondition(params, true);
  }

  return NULL;
}

sql::Connection* getMyDSMSessionConnection(DSMSession* sc_sess) {
  if (sc_sess->avar.find(MY_AKEY_CONNECTION) == sc_sess->avar.end()) {
    sc_sess->SET_ERRNO(DSM_ERRNO_MY_CONNECTION);
    sc_sess->SET_STRERROR("No connection to database");
    return NULL;
  }
  AmObject* ao = NULL;
  sql::Connection* res = NULL;
  try {
    if (!isArgAObject(sc_sess->avar[MY_AKEY_CONNECTION])) {
      sc_sess->SET_ERRNO(DSM_ERRNO_MY_CONNECTION);
      sc_sess->SET_STRERROR("No connection to database (not AmObject)");
      return NULL;
    }
    ao = (sc_sess->avar[MY_AKEY_CONNECTION]).asObject();
  } catch (...){
    sc_sess->SET_ERRNO(DSM_ERRNO_MY_CONNECTION);
    sc_sess->SET_STRERROR("No connection to database (not AmObject)");
    return NULL;
  }
  if (NULL != ao) {
    DSMMyConnection* dsm_conn = dynamic_cast<DSMMyConnection*>(ao);
    if (NULL != dsm_conn) {
      res = dsm_conn->con;
      return res;
    }
  }
  sc_sess->SET_ERRNO(DSM_ERRNO_MY_CONNECTION);
  sc_sess->SET_STRERROR("No connection to database (no sql::Connection)");
  return NULL;
}

sql::ResultSet* getMyDSMQueryResult(DSMSession* sc_sess) {
  if (sc_sess->avar.find(MY_AKEY_RESULT) == sc_sess->avar.end()) {
    sc_sess->SET_ERRNO(DSM_ERRNO_MY_NORESULT);
    sc_sess->SET_STRERROR("No result available");
    return NULL;
  }
  AmObject* ao = NULL;
  sql::ResultSet* res = NULL;
  try {
    assertArgAObject(sc_sess->avar[MY_AKEY_RESULT]);
    ao = sc_sess->avar[MY_AKEY_RESULT].asObject();
  } catch (...){
    sc_sess->SET_ERRNO(DSM_ERRNO_MY_NORESULT);
    sc_sess->SET_STRERROR("Result object has wrong type");
    return NULL;
  }

  if (NULL == ao || NULL == (res = dynamic_cast<sql::ResultSet*>(ao))) {
    sc_sess->SET_STRERROR("Result object has wrong type");
    sc_sess->SET_ERRNO(DSM_ERRNO_MY_NORESULT);
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

string str_between(const string s, char b, char e) {
  size_t pos1 = s.find(b);
  if (b == '\0' || pos1 == string::npos)
    pos1 = 0;
  else
    pos1++;
  size_t pos2 = s.find(e, pos1);
  if (e == '\0' || pos2 == string::npos)
    pos2 = s.length();
  return s.substr(pos1, pos2-pos1);
}

EXEC_ACTION_START(SCMyConnectAction) {
  string f_arg = resolveVars(arg, sess, sc_sess, event_params);
  string db_url = f_arg.length()?f_arg:sc_sess->var["config.db_url"];
  if (db_url.empty() || db_url.length() < 11 || db_url.substr(0, 8) != "mysql://") {
    ERROR("missing correct db_url config or connect parameter\n");
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("missing correct db_url config or connect parameter\n");
    return false;
  }
  // split url
  db_url = db_url.substr(8); 
  string db_user = str_between(db_url, '\0', ':');
  string db_pwd  = str_between(db_url,  ':', '@');
  string db_host = str_between(db_url,  '@', '/');
  string db_db   = str_between(db_url,  '/', '\0');

  string db_ca_cert = sc_sess->var["config.mysql_ca_cert"];
  if (!db_ca_cert.empty() && (access(db_ca_cert.c_str(), R_OK ) != 0)) {
    ERROR("cannot access mysql_ca_cert file %s\n", db_ca_cert.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_CONFIG);
    sc_sess->SET_STRERROR("cannot access mysql_ca_cert file\n");
    return false;
  }

  try {
    bool reconnect_state = true;
    sql::ConnectOptionsMap connection_properties;
    sql::Driver *driver;
    DSMMyConnection* conn = NULL;
    connection_properties["hostName"] = sql::ConnectPropertyVal(db_host);
    connection_properties["userName"] = sql::ConnectPropertyVal(db_user);
    connection_properties["password"] = sql::ConnectPropertyVal(db_pwd);
    if (!db_ca_cert.empty()) {
      connection_properties["sslCa"] =
	sql::ConnectPropertyVal(sql::SQLString(db_ca_cert));
      connection_properties["sslCAPath"] =
	sql::ConnectPropertyVal(sql::SQLString(""));
      connection_properties["sslCipher"] =
	sql::ConnectPropertyVal(sql::SQLString("DHE-RSA-AES256-SHA"));
      connection_properties["sslEnforce"] =
	sql::ConnectPropertyVal(true);
    }
    driver = get_driver_instance();
    conn = new DSMMyConnection();
    conn->con = driver->connect(connection_properties);
    conn->con->setClientOption("OPT_RECONNECT", &reconnect_state);
    conn->con->setSchema(db_db);
    // save connection for later use
    AmArg c_arg_con;
    c_arg_con.setBorrowedPointer(conn);
    sc_sess->avar[MY_AKEY_CONNECTION] = c_arg_con;
    // for garbage collection
    sc_sess->transferOwnership(conn);
    sc_sess->CLR_ERRNO;    
  } catch (const sql::SQLException& e) {
    ERROR("DB connection failed with error %d: '%s'\n", e.getErrorCode(),
	  e.what());
    sc_sess->SET_ERRNO(DSM_ERRNO_MY_CONNECTION);
    sc_sess->SET_STRERROR(e.what());
    sc_sess->var["db.errno"] = int2str(e.getErrorCode());
    sc_sess->var["db.ereason"] = e.what();
  }

} EXEC_ACTION_END;

EXEC_ACTION_START(SCMyDisconnectAction) {
  sql::Connection* conn = getMyDSMSessionConnection(sc_sess);
  if (NULL == conn) 
    return false;

  try {
    if (!conn->isClosed()) conn->close();
    // connection object might be reused - but its safer to create a new one
    sc_sess->avar[MY_AKEY_CONNECTION] = AmArg();
    sc_sess->CLR_ERRNO;
  } catch (const sql::SQLException& e) {
    ERROR("DB disconnect failed: '%s'\n", e.what());
    sc_sess->SET_ERRNO(DSM_ERRNO_MY_CONNECTION);
    sc_sess->SET_STRERROR(e.what());
    sc_sess->var["db.ereason"] = e.what();
  }
} EXEC_ACTION_END;

EXEC_ACTION_START(SCMyResolveQueryParams) {
  sc_sess->var["db.qstr"] = 
    replaceQueryParams(arg, sc_sess, event_params);
} EXEC_ACTION_END;

unsigned long getInsertId(sql::Connection* conn) {
  try {
    sql::Statement* stmt = conn->createStatement();
    sql::ResultSet* res = stmt->executeQuery("SELECT @@identity AS id");
    unsigned long retVal = 0;
    if (res->next()) {
      retVal = res->getInt64("id");
    } else {
      ERROR("DB query 'SELECT @@identity AS id' gave no result'\n");
    }
    delete stmt;
    delete res;
    return retVal;
  } catch (const sql::SQLException& e) {
    ERROR("DB query 'SELECT @@identity AS id' failed: '%s'\n", e.what());
    return 0;
  }
}

EXEC_ACTION_START(SCMyExecuteAction) {
  sql::Connection* conn = getMyDSMSessionConnection(sc_sess);
  if (NULL == conn) 
    return false;
  string qstr = replaceQueryParams(arg, sc_sess, event_params);

  try {
    sql::Statement* stmt = conn->createStatement();
    sql::ResultSet* res = stmt->executeQuery(qstr);
    if (res) {
      sc_sess->CLR_ERRNO;
      sc_sess->var["db.rows"] = int2str((int)res->rowsCount());
      sc_sess->var["db.info"] = "";
      sc_sess->var["db.insert_id"] = int2str((unsigned int)getInsertId(conn));
    } else {
      sc_sess->SET_ERRNO(DSM_ERRNO_MY_QUERY);
      sc_sess->SET_STRERROR("query did not have a result");
      sc_sess->var["db.info"] = "";
    }
    delete stmt;
    delete res;
  } catch (const sql::SQLException& e) {
    ERROR("DB query '%s' failed: '%s'\n", qstr.c_str(), e.what());
    sc_sess->SET_ERRNO(DSM_ERRNO_MY_QUERY);    
    sc_sess->SET_STRERROR(e.what());
    sc_sess->var["db.ereason"] = e.what();
  }
} EXEC_ACTION_END;

EXEC_ACTION_START(SCMyQueryAction) {
  sql::Connection* conn = getMyDSMSessionConnection(sc_sess);
  if (NULL == conn)
    return false;
  string qstr = replaceQueryParams(arg, sc_sess, event_params);

  try {
    DSMMyStoreQueryResult* m_res = new DSMMyStoreQueryResult();
    sql::Statement* stmt = conn->createStatement();
    m_res->res = stmt->executeQuery(qstr);
    if (m_res->res) {
      AmArg c_arg;
      c_arg.setBorrowedPointer(m_res);
      sc_sess->avar[MY_AKEY_RESULT] = c_arg;
      // for garbage collection
      sc_sess->transferOwnership(m_res);

      sc_sess->CLR_ERRNO;
      sc_sess->var["db.rows"] = int2str((unsigned int)m_res->res->rowsCount());
    } else {
      sc_sess->SET_ERRNO(DSM_ERRNO_MY_QUERY);
      sc_sess->SET_STRERROR("query did not have a result");
    }
    delete stmt;
  } catch (const sql::SQLException& e) {
    ERROR("DB query '%s' failed: '%s'\n", qstr.c_str(), e.what());
    sc_sess->SET_ERRNO(DSM_ERRNO_MY_QUERY);    
    sc_sess->SET_STRERROR(e.what());
    sc_sess->var["db.ereason"] = e.what();
  }
} EXEC_ACTION_END;

CONST_ACTION_2P(SCMyQueryGetResultAction, ',', true);
EXEC_ACTION_START(SCMyQueryGetResultAction) {
  sql::Connection* conn = getMyDSMSessionConnection(sc_sess);
  if (NULL == conn) 
    return false;
  string qstr = replaceQueryParams(par1, sc_sess, event_params);

  try {
    sql::Statement* stmt = conn->createStatement();
    sql::ResultSet* res = stmt->executeQuery(qstr);
    if (res) {
      unsigned int rowindex_i = 0;
      string rowindex = resolveVars(par2, sess, sc_sess, event_params);
      if (rowindex.length()) {
	if (str2i(rowindex, rowindex_i)) {
	  ERROR("row index '%s' not understood\n", rowindex.c_str());
	  sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
	  sc_sess->SET_STRERROR("row index '"+rowindex+"' not understood\n");
	  return false;
	}
      }
      size_t num_rows = res->rowsCount();
      if (num_rows <= rowindex_i) {
	sc_sess->SET_ERRNO(DSM_ERRNO_MY_NOROW);
	sc_sess->SET_STRERROR("row index out of result rows bounds");
	delete stmt;
	delete res;
	return false;
      }

      // get all columns
      sql::ResultSetMetaData* meta = res->getMetaData();
      for (size_t i = 0; i <= rowindex_i; i++) res->next();
      for (size_t i = 0; i < meta->getColumnCount(); i++) {
	sc_sess->var[meta->getColumnLabel(i)] =
	  (res->getString(i)).asStdString();
      }

      sc_sess->CLR_ERRNO;    
      sc_sess->var["db.rows"] = int2str((int)num_rows);
      delete res;
    } else {
      sc_sess->SET_ERRNO(DSM_ERRNO_MY_QUERY);
    }
    delete stmt;
  } catch (const sql::SQLException& e) {
    ERROR("DB query '%s' failed: '%s'\n", qstr.c_str(), e.what());
    sc_sess->SET_ERRNO(DSM_ERRNO_MY_QUERY);    
    sc_sess->SET_STRERROR(e.what());
    sc_sess->var["db.ereason"] = e.what();
  }
} EXEC_ACTION_END;

CONST_ACTION_2P(SCMyGetResultAction, ',', true);
EXEC_ACTION_START(SCMyGetResultAction) {
  sql::ResultSet* res = getMyDSMQueryResult(sc_sess);
  if (NULL == res)
    return false;

  if (!res) {
    sc_sess->SET_ERRNO(DSM_ERRNO_MY_NORESULT);
    sc_sess->SET_STRERROR("No result from query");
    return false;
  }

  unsigned int rowindex_i = 0;
  string rowindex = resolveVars(par1, sess, sc_sess, event_params);
  string colname  = resolveVars(par2, sess, sc_sess, event_params);

  if (rowindex.length()) {
    if (str2i(rowindex, rowindex_i)) {
      ERROR("row index '%s' not understood\n", rowindex.c_str());
      sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
      sc_sess->SET_STRERROR("row index '"+rowindex+"' not understood");
      return false;
    }
  }

  if (res->rowsCount() <= rowindex_i) {
    sc_sess->SET_ERRNO(DSM_ERRNO_MY_NOROW);
    sc_sess->SET_STRERROR("row index out of result rows bounds");
    return false;
  }
  DBG("rowindex_i = %d\n", rowindex_i);
  if (colname.length()) {
    sql::SQLString colLabel = sql::SQLString(colname);
    // get only this column
    for (size_t i = 0; i <= rowindex_i; i++) res->next();
    try {
      sc_sess->var[colname] = (res->getString(colLabel)).asStdString();
    } catch (const sql::SQLException& e) {
      sc_sess->SET_ERRNO(DSM_ERRNO_MY_NOCOLUMN);
      sc_sess->SET_STRERROR("bad field name '"+colname+"'");
      return false;
    }
  } else {
    // get all columns
    sql::ResultSetMetaData* meta = res->getMetaData();
    for (size_t i = 0; i <= rowindex_i; i++) res->next();
    for (size_t i = 0; i < meta->getColumnCount(); i++) {
      sc_sess->var[(meta->getColumnLabel(i)).asStdString()] =
	(res->getString(i)).asStdString();
    }
  }
  sc_sess->CLR_ERRNO;
} EXEC_ACTION_END;

EXEC_ACTION_START(SCMyGetClientVersion) {
  sql::Connection* conn = getMyDSMSessionConnection(sc_sess);
  if (NULL == conn)
    return false;

  sc_sess->var[resolveVars(arg, sess, sc_sess, event_params)] = 
    (conn->getClientInfo()).asStdString();
  sc_sess->CLR_ERRNO;
} EXEC_ACTION_END;

MATCH_CONDITION_START(MyHasResultCondition) {
  sql::ResultSet* res = getMyDSMQueryResult(sc_sess);
  if (NULL == res)
    return false;

  if (!res || !(res->rowsCount())) {
    return false;
  }

  return true;
} MATCH_CONDITION_END;

MATCH_CONDITION_START(MyConnectedCondition) {
  sql::Connection* conn = getMyDSMSessionConnection(sc_sess);
  if (NULL == conn) 
    return false;

  return conn->isValid() && !conn->isClosed();
} MATCH_CONDITION_END;


EXEC_ACTION_START(SCMySaveResultAction) {
  sc_sess->avar[resolveVars(arg, sess, sc_sess, event_params)] = sc_sess->avar[MY_AKEY_RESULT];
} EXEC_ACTION_END;

EXEC_ACTION_START(SCMyUseResultAction) {
  sc_sess->avar[MY_AKEY_RESULT] = sc_sess->avar[resolveVars(arg, sess, sc_sess, event_params)];
} EXEC_ACTION_END;


bool playDBAudio(AmSession* sess, DSMSession* sc_sess, DSMCondition::EventType event,
		 map<string,string>* event_params, const string& par1, const string& par2,
		 bool looped, bool front) {
  sql::Connection* conn = getMyDSMSessionConnection(sc_sess);
  if (NULL == conn)
    EXEC_ACTION_STOP;

  string qstr = replaceQueryParams(par1, sc_sess, event_params);

  INFO("playDBAudio query '%s'\n", qstr.c_str());
  try {
    sql::Statement* stmt = conn->createStatement();
    sql::ResultSet* res = stmt->executeQuery(qstr);
    if (res) {

      if (!res->next()) {
	sc_sess->SET_ERRNO(DSM_ERRNO_MY_NOROW);
	sc_sess->SET_STRERROR("result does not have row");
	EXEC_ACTION_STOP;
      }
      if (res->getMetaData()->getColumnCount() == 0) {
	sc_sess->SET_ERRNO(DSM_ERRNO_MY_NODATA);
	sc_sess->SET_STRERROR("result does not have data");
	EXEC_ACTION_STOP;
      }
      FILE *t_file = tmpfile();
      if (NULL == t_file) {
	sc_sess->SET_ERRNO(DSM_ERRNO_FILE);
	sc_sess->SET_STRERROR("tmpfile() failed: "+string(strerror(errno)));
	EXEC_ACTION_STOP;
      }
      string s = res->getString(0);
      fwrite(s.data(), 1, s.length(), t_file);
      rewind(t_file);
      delete res;
      
      DSMDisposableAudioFile* a_file = new DSMDisposableAudioFile();
      if (a_file->fpopen(par2, AmAudioFile::Read, t_file)) {
	sc_sess->SET_ERRNO(DSM_ERRNO_FILE);
	sc_sess->SET_STRERROR("fpopen failed!");
	EXEC_ACTION_STOP;
      }

      a_file->loop.set(looped);

      sc_sess->addToPlaylist(new AmPlaylistItem(a_file, NULL), front);
      sc_sess->transferOwnership(a_file);

      sc_sess->CLR_ERRNO;    
    } else {
      sc_sess->SET_ERRNO(DSM_ERRNO_MY_QUERY);
      sc_sess->SET_STRERROR("query does not have result"); 
    }
    delete stmt;
  } catch (const sql::SQLException& e) {
    ERROR("DB query '%s' failed: '%s'\n", qstr.c_str(), e.what());
    sc_sess->SET_ERRNO(DSM_ERRNO_MY_QUERY);    
    sc_sess->SET_STRERROR(e.what());
    sc_sess->var["db.ereason"] = e.what();
  }
  return false;
}

CONST_ACTION_2P(SCMyPlayDBAudioAction, ',', true);
EXEC_ACTION_START(SCMyPlayDBAudioAction) {
  playDBAudio(sess, sc_sess, event, event_params, par1, par2,
	      /*looped = */ false, /*front = */ false);
} EXEC_ACTION_END;

CONST_ACTION_2P(SCMyPlayDBAudioFrontAction, ',', true);
EXEC_ACTION_START(SCMyPlayDBAudioFrontAction) {
  playDBAudio(sess, sc_sess, event, event_params, par1, par2,
	      /*looped = */ false, /*front = */ true);
} EXEC_ACTION_END;

CONST_ACTION_2P(SCMyPlayDBAudioLoopedAction, ',', true);
EXEC_ACTION_START(SCMyPlayDBAudioLoopedAction) {
  playDBAudio(sess, sc_sess, event, event_params, par1, par2,
	      /*looped = */ true, /*front = */ false);
} EXEC_ACTION_END;

CONST_ACTION_2P(SCMyGetFileFromDBAction, ',', true);
EXEC_ACTION_START(SCMyGetFileFromDBAction) {
  sql::Connection* conn = getMyDSMSessionConnection(sc_sess);
  if (NULL == conn) 
    return false;
  string qstr = replaceQueryParams(par1, sc_sess, event_params);
  string fname = resolveVars(par2, sess, sc_sess, event_params);

  try {
    sql::Statement* stmt = conn->createStatement();
    sql::ResultSet* res = stmt->executeQuery(qstr);
    if (res) {
      if (!res->next()) {
	sc_sess->SET_ERRNO(DSM_ERRNO_MY_NOROW);
	sc_sess->SET_STRERROR("result does not have row");
	delete stmt;
	delete res;
	return false;
      }
      FILE *t_file = fopen(fname.c_str(), "wb");
      if (NULL == t_file) {
	sc_sess->SET_ERRNO(DSM_ERRNO_FILE);
	sc_sess->SET_STRERROR("fopen() failed for file '"+fname+"': "+string(strerror(errno)));
	delete stmt;
	delete res;
	return false;
      }

      string s = res->getString(0);
      fwrite(s.data(), 1, s.length(), t_file);
      fclose(t_file);
      delete res;

      sc_sess->CLR_ERRNO;    
    } else {
      sc_sess->SET_ERRNO(DSM_ERRNO_MY_QUERY);
      sc_sess->SET_STRERROR("query does not have result"); 
    }
    delete stmt;
  } catch (const sql::SQLException& e) {
    ERROR("DB query '%s' failed: '%s'\n", qstr.c_str(), e.what());
    sc_sess->SET_ERRNO(DSM_ERRNO_MY_QUERY);
    sc_sess->SET_STRERROR(e.what());
    sc_sess->var["db.ereason"] = e.what();
  }
} EXEC_ACTION_END;

CONST_ACTION_2P(SCMyPutFileToDBAction, ',', true);
EXEC_ACTION_START(SCMyPutFileToDBAction) {
  sql::Connection* conn = getMyDSMSessionConnection(sc_sess);
  if (NULL == conn) 
    return false;
  string qstr = replaceQueryParams(par1, sc_sess, event_params);

  string fname = resolveVars(par2, sess, sc_sess, event_params);

  size_t fpos = qstr.find("__FILE__");
  if (fpos == string::npos) {
    ERROR("missing __FILE__ in query string '%s'\n", 
	  par1.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("missing __FILE__ in query string '"+par1+"'\n");
    return false;
  }
  
  try {
    std::ifstream data_file(fname.c_str(), std::ios::in | std::ios::binary);
    if (!data_file) {
      DBG("could not read file '%s'\n", fname.c_str());
      sc_sess->SET_ERRNO(DSM_ERRNO_FILE);
      sc_sess->SET_STRERROR("could not read file '"+fname+"'\n");
      return false;
    }
    // that one is clever...
    // (see http://www.gamedev.net/community/forums/topic.asp?topic_id=353162 )
    string file_data((std::istreambuf_iterator<char>(data_file)), 
		     std::istreambuf_iterator<char>());
       
    if (file_data.empty()) {
      DBG("could not read file '%s'\n", fname.c_str());
      sc_sess->SET_ERRNO(DSM_ERRNO_FILE);
      sc_sess->SET_STRERROR("could not read file '"+fname+"'\n");
      return false;
    }

    sql::mysql::MySQL_Connection* mysql_conn =
      dynamic_cast<sql::mysql::MySQL_Connection*>(conn);
    string query = qstr.substr(0, fpos) +
      mysql_conn->escapeString(sql::SQLString(file_data)) +
      qstr.substr(fpos+8);

    sql::Statement* stmt = conn->createStatement();
    sql::ResultSet* res = stmt->executeQuery(query);
    if (res) {
      sc_sess->CLR_ERRNO;
      sc_sess->var["db.rows"] = int2str((int)res->rowsCount());
      sc_sess->var["db.info"] = "";
      sc_sess->var["db.insert_id"] = int2str((unsigned int)getInsertId(conn));
      delete res;
    } else {
      sc_sess->SET_ERRNO(DSM_ERRNO_MY_QUERY);
      sc_sess->SET_STRERROR("query did not have a result");
      sc_sess->var["db.info"] = "";
    }

    delete stmt;
  } catch (const sql::SQLException& e) {
    ERROR("DB query '%s' failed: '%s'\n", par1.c_str(), e.what());
    sc_sess->SET_ERRNO(DSM_ERRNO_MY_QUERY);    
    sc_sess->SET_STRERROR(e.what());
    sc_sess->var["db.ereason"] = e.what();
  }
} EXEC_ACTION_END;

CONST_ACTION_2P(SCMyEscapeAction, '=', false);
EXEC_ACTION_START(SCMyEscapeAction) {
  sql::mysql::MySQL_Connection* mysql_conn =
    dynamic_cast<sql::mysql::MySQL_Connection*>(getMyDSMSessionConnection(sc_sess));

  if (NULL == mysql_conn)
    return false;

  string val = resolveVars(par2, sess, sc_sess, event_params);

  string dstvar = par1;
  if (dstvar.size() && dstvar[0] == '$') {
    dstvar = dstvar.substr(1);
  }
  string res = mysql_conn->escapeString(sql::SQLString(val));
  sc_sess->var[dstvar] = res;
  DBG("escaped: $%s = escape(%s) = %s\n",
      dstvar.c_str(), val.c_str(), res.c_str());
} EXEC_ACTION_END;
