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

mysqlpp::Connection* getMyDSMSessionConnection(DSMSession* sc_sess) {
  if (sc_sess->avar.find(MY_AKEY_CONNECTION) == sc_sess->avar.end()) {
    sc_sess->SET_ERRNO(DSM_ERRNO_MY_CONNECTION);
    sc_sess->SET_STRERROR("No connection to database");
    return NULL;
  }
  AmObject* ao = NULL; mysqlpp::Connection* res = NULL;
  try {
    if (!isArgAObject(sc_sess->avar[MY_AKEY_CONNECTION])) {
      sc_sess->SET_ERRNO(DSM_ERRNO_MY_CONNECTION);
      sc_sess->SET_STRERROR("No connection to database (not AmObject)");
      return NULL;
    }
    ao = sc_sess->avar[MY_AKEY_CONNECTION].asObject();
  } catch (...){
    sc_sess->SET_ERRNO(DSM_ERRNO_MY_CONNECTION);
    sc_sess->SET_STRERROR("No connection to database (not AmObject)");
    return NULL;
  }

  if (NULL == ao || NULL == (res = dynamic_cast<mysqlpp::Connection*>(ao))) {
    sc_sess->SET_ERRNO(DSM_ERRNO_MY_CONNECTION);
    sc_sess->SET_STRERROR("No connection to database (not mysqlpp::Connection)");
    return NULL;
  }
  return res;
}

mysqlpp::StoreQueryResult* getMyDSMQueryResult(DSMSession* sc_sess) {
  if (sc_sess->avar.find(MY_AKEY_RESULT) == sc_sess->avar.end()) {
    sc_sess->SET_ERRNO(DSM_ERRNO_MY_NORESULT);
    sc_sess->SET_STRERROR("No result available");
    return NULL;
  }
  AmObject* ao = NULL; mysqlpp::StoreQueryResult* res = NULL;
  try {
    assertArgAObject(sc_sess->avar[MY_AKEY_RESULT]);
    ao = sc_sess->avar[MY_AKEY_RESULT].asObject();
  } catch (...){
    sc_sess->SET_ERRNO(DSM_ERRNO_MY_NORESULT);
    sc_sess->SET_STRERROR("Result object has wrong type");
    return NULL;
  }

  if (NULL == ao || NULL == (res = dynamic_cast<mysqlpp::StoreQueryResult*>(ao))) {
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

  DSMMyConnection* conn = NULL;
  try {
    conn = new DSMMyConnection(db_db.c_str(), db_host.c_str(), db_user.c_str(), db_pwd.c_str());

    // save connection for later use
    AmArg c_arg;
    c_arg.setBorrowedPointer(conn);
    sc_sess->avar[MY_AKEY_CONNECTION] = c_arg;
    // for garbage collection
    sc_sess->transferOwnership(conn);
    sc_sess->CLR_ERRNO;    
  } catch (const mysqlpp::ConnectionFailed& e) {
    ERROR("DB connection failed with error %d: '%s'\n",e.errnum(), e.what());
    sc_sess->SET_ERRNO(DSM_ERRNO_MY_CONNECTION);
    sc_sess->SET_STRERROR(e.what());
    sc_sess->var["db.errno"] = int2str(e.errnum());
    sc_sess->var["db.ereason"] = e.what();
  } catch (const mysqlpp::Exception& e) {
    ERROR("DB connection failed: '%s'\n", e.what());
    sc_sess->SET_ERRNO(DSM_ERRNO_MY_CONNECTION);    
    sc_sess->SET_STRERROR(e.what());
    sc_sess->var["db.ereason"] = e.what();
  }

} EXEC_ACTION_END;

EXEC_ACTION_START(SCMyDisconnectAction) {
  mysqlpp::Connection* conn = 
    getMyDSMSessionConnection(sc_sess);
  if (NULL == conn) 
    return false;

  try {
    conn->disconnect();
    // connection object might be reused - but its safer to create a new one
    sc_sess->avar[MY_AKEY_CONNECTION] = AmArg();
    sc_sess->CLR_ERRNO;
  } catch (const mysqlpp::Exception& e) {
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

EXEC_ACTION_START(SCMyExecuteAction) {
  mysqlpp::Connection* conn = 
    getMyDSMSessionConnection(sc_sess);
  if (NULL == conn) 
    return false;
  string qstr = replaceQueryParams(arg, sc_sess, event_params);

  try {
    mysqlpp::Query query = conn->query(qstr.c_str());
    mysqlpp::SimpleResult res = query.execute();
    if (res) {
      sc_sess->CLR_ERRNO;
      sc_sess->var["db.rows"] = int2str((int)res.rows());
      sc_sess->var["db.info"] = res.info();
      sc_sess->var["db.insert_id"] = int2str((int)res.insert_id());
    } else {
      sc_sess->SET_ERRNO(DSM_ERRNO_MY_QUERY);
      sc_sess->SET_STRERROR(res.info());
      sc_sess->var["db.info"] = res.info();
    }

  } catch (const mysqlpp::Exception& e) {
    ERROR("DB query '%s' failed: '%s'\n", 
	  qstr.c_str(), e.what());
    sc_sess->SET_ERRNO(DSM_ERRNO_MY_QUERY);    
    sc_sess->SET_STRERROR(e.what());
    sc_sess->var["db.ereason"] = e.what();
  }
} EXEC_ACTION_END;

EXEC_ACTION_START(SCMyQueryAction) {
  mysqlpp::Connection* conn = 
    getMyDSMSessionConnection(sc_sess);
  if (NULL == conn) 
    return false;
  string qstr = replaceQueryParams(arg, sc_sess, event_params);

  try {
    mysqlpp::Query query = conn->query(qstr.c_str());
    mysqlpp::StoreQueryResult res = query.store();    
    if (res) {
      // MySQL++ does not allow working with pointers here, so copy construct it
      DSMMyStoreQueryResult* m_res = new DSMMyStoreQueryResult(res);

      // save result for later use
      AmArg c_arg;
      c_arg.setBorrowedPointer(m_res);
      sc_sess->avar[MY_AKEY_RESULT] = c_arg;

      // for garbage collection
      sc_sess->transferOwnership(m_res);

      sc_sess->CLR_ERRNO;    
      sc_sess->var["db.rows"] = int2str((unsigned int)res.num_rows());
    } else {
      sc_sess->SET_ERRNO(DSM_ERRNO_MY_QUERY);
      sc_sess->SET_STRERROR("query did not have a result");
    }
  } catch (const mysqlpp::Exception& e) {
    ERROR("DB query '%s' failed: '%s'\n", 
	  qstr.c_str(), e.what());
    sc_sess->SET_ERRNO(DSM_ERRNO_MY_QUERY);    
    sc_sess->SET_STRERROR(e.what());
    sc_sess->var["db.ereason"] = e.what();
  }
} EXEC_ACTION_END;

CONST_ACTION_2P(SCMyQueryGetResultAction, ',', true);
EXEC_ACTION_START(SCMyQueryGetResultAction) {
  mysqlpp::Connection* conn = 
    getMyDSMSessionConnection(sc_sess);
  if (NULL == conn) 
    return false;
  string qstr = replaceQueryParams(par1, sc_sess, event_params);

  try {
    mysqlpp::Query query = conn->query(qstr.c_str());
    mysqlpp::StoreQueryResult res = query.store();    
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
      if (res.size() <= rowindex_i) {
	sc_sess->SET_ERRNO(DSM_ERRNO_MY_NOROW);
	sc_sess->SET_STRERROR("row index out of result rows bounds"); 
	return false;
      }

      // get all columns
      for (size_t i = 0; i < res.field_names()->size(); i++) {
	sc_sess->var[res.field_name(i)] = 
	  (string)res[rowindex_i][res.field_name(i).c_str()];
      }

      sc_sess->CLR_ERRNO;    
      sc_sess->var["db.rows"] = int2str((unsigned int)res.num_rows());
    } else {
      sc_sess->SET_ERRNO(DSM_ERRNO_MY_QUERY);
    }
  } catch (const mysqlpp::Exception& e) {
    ERROR("DB query '%s' failed: '%s'\n", 
	  qstr.c_str(), e.what());
    sc_sess->SET_ERRNO(DSM_ERRNO_MY_QUERY);    
    sc_sess->SET_STRERROR(e.what());
    sc_sess->var["db.ereason"] = e.what();
  }
} EXEC_ACTION_END;

CONST_ACTION_2P(SCMyGetResultAction, ',', true);
EXEC_ACTION_START(SCMyGetResultAction) {
  mysqlpp::StoreQueryResult* res = getMyDSMQueryResult(sc_sess);
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

  if (res->size() <= rowindex_i) {
    sc_sess->SET_ERRNO(DSM_ERRNO_MY_NOROW);
    sc_sess->SET_STRERROR("row index out of result rows bounds");
    return false;
  }
  DBG("rowindex_i = %d\n", rowindex_i);
  if (colname.length()) {
    // get only this column
    try {      
      sc_sess->var[colname] = 
	(string)(*res)[(int)rowindex_i][colname.c_str()];
    } catch (const mysqlpp::BadFieldName& e) {
      sc_sess->SET_ERRNO(DSM_ERRNO_MY_NOCOLUMN);
      sc_sess->SET_STRERROR("bad field name '"+colname+"'");
      return false;
    }
  } else {
    // get all columns
    for (size_t i = 0; i < res->field_names()->size(); i++) {
      sc_sess->var[res->field_name(i)] = 
	(string)(*res)[rowindex_i][res->field_name(i).c_str()];
    }
  }
  sc_sess->CLR_ERRNO;
} EXEC_ACTION_END;

EXEC_ACTION_START(SCMyGetClientVersion) {
  mysqlpp::Connection* conn = 
    getMyDSMSessionConnection(sc_sess);
  if (NULL == conn) 
    return false;

  sc_sess->var[resolveVars(arg, sess, sc_sess, event_params)] = 
    conn->client_version();
  sc_sess->CLR_ERRNO;
} EXEC_ACTION_END;

MATCH_CONDITION_START(MyHasResultCondition) {
  mysqlpp::StoreQueryResult* res = getMyDSMQueryResult(sc_sess);
  if (NULL == res)
    return false;

  if (!res || !(res->size())) {
    return false;
  }

  return true;
} MATCH_CONDITION_END;

MATCH_CONDITION_START(MyConnectedCondition) {
  mysqlpp::Connection* conn = 
    getMyDSMSessionConnection(sc_sess);
  if (NULL == conn) 
    return false;

  return conn->connected();
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
  mysqlpp::Connection* conn = 
    getMyDSMSessionConnection(sc_sess);
  if (NULL == conn)
    EXEC_ACTION_STOP;

  string qstr = replaceQueryParams(par1, sc_sess, event_params);

  try {
    mysqlpp::Query query = conn->query(qstr.c_str());
    mysqlpp::UseQueryResult res = query.use();    
    if (res) {

      mysqlpp::Row row = res.fetch_row();
      if (!row) {
	sc_sess->SET_ERRNO(DSM_ERRNO_MY_NOROW);
	sc_sess->SET_STRERROR("result does not have row");
	EXEC_ACTION_STOP;
      }
      FILE *t_file = tmpfile();
      if (NULL == t_file) {
	sc_sess->SET_ERRNO(DSM_ERRNO_FILE);
	sc_sess->SET_STRERROR("tmpfile() failed: "+string(strerror(errno)));
	EXEC_ACTION_STOP;
      }

      fwrite(row.at(0).data(), 1, row.at(0).size(), t_file);
      rewind(t_file);
      
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
  } catch (const mysqlpp::Exception& e) {
    ERROR("DB query '%s' failed: '%s'\n", 
	  qstr.c_str(), e.what());
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
  mysqlpp::Connection* conn = 
    getMyDSMSessionConnection(sc_sess);
  if (NULL == conn) 
    return false;
  string qstr = replaceQueryParams(par1, sc_sess, event_params);
  string fname = resolveVars(par2, sess, sc_sess, event_params);

  try {
    mysqlpp::Query query = conn->query(qstr.c_str());
    mysqlpp::UseQueryResult res = query.use();    
    if (res) {
      mysqlpp::Row row = res.fetch_row();
      if (!row) {
	sc_sess->SET_ERRNO(DSM_ERRNO_MY_NOROW);
	sc_sess->SET_STRERROR("result does not have row"); 
	return false;
      }
      FILE *t_file = fopen(fname.c_str(), "wb");
      if (NULL == t_file) {
	sc_sess->SET_ERRNO(DSM_ERRNO_FILE);
	sc_sess->SET_STRERROR("fopen() failed for file '"+fname+"': "+string(strerror(errno)));
	return false;
      }

      fwrite(row.at(0).data(), 1, row.at(0).size(), t_file);
      fclose(t_file);

      sc_sess->CLR_ERRNO;    
    } else {
      sc_sess->SET_ERRNO(DSM_ERRNO_MY_QUERY);
      sc_sess->SET_STRERROR("query does not have result"); 
    }
  } catch (const mysqlpp::Exception& e) {
    ERROR("DB query '%s' failed: '%s'\n", 
	  qstr.c_str(), e.what());
    sc_sess->SET_ERRNO(DSM_ERRNO_MY_QUERY);
    sc_sess->SET_STRERROR(e.what());
    sc_sess->var["db.ereason"] = e.what();
  }
} EXEC_ACTION_END;

CONST_ACTION_2P(SCMyPutFileToDBAction, ',', true);
EXEC_ACTION_START(SCMyPutFileToDBAction) {
  mysqlpp::Connection* conn = 
    getMyDSMSessionConnection(sc_sess);
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

    mysqlpp::Query query = conn->query();
    query << qstr.substr(0, fpos) <<  
      mysqlpp::escape << file_data << qstr.substr(fpos+8);

    mysqlpp::SimpleResult res = query.execute();
    if (res) {
      sc_sess->CLR_ERRNO;
      sc_sess->var["db.rows"] = int2str((int)res.rows());
      sc_sess->var["db.info"] = res.info();
      sc_sess->var["db.insert_id"] = int2str((int)res.insert_id());
    } else {
      sc_sess->SET_ERRNO(DSM_ERRNO_MY_QUERY);
      sc_sess->SET_STRERROR(res.info());
      sc_sess->var["db.info"] = res.info();
    }

  } catch (const mysqlpp::Exception& e) {
    ERROR("DB query '%s' failed: '%s'\n", 
	  par1.c_str(), e.what());
    sc_sess->SET_ERRNO(DSM_ERRNO_MY_QUERY);    
    sc_sess->SET_STRERROR(e.what());
    sc_sess->var["db.ereason"] = e.what();
  }
} EXEC_ACTION_END;

CONST_ACTION_2P(SCMyEscapeAction, '=', false);
EXEC_ACTION_START(SCMyEscapeAction) {
  mysqlpp::Connection* conn =
    getMyDSMSessionConnection(sc_sess);

  if (NULL == conn)
    return false;

  mysqlpp::Query query = conn->query();

  string val = resolveVars(par2, sess, sc_sess, event_params);

  string dstvar = par1;
  if (dstvar.size() && dstvar[0] == '$') {
    dstvar = dstvar.substr(1);
  }
  string res;
  query.escape_string(&res, val.c_str(), val.length());
  sc_sess->var[dstvar] = res;
  DBG("escaped: $%s = escape(%s) = %s\n",
      dstvar.c_str(), val.c_str(), res.c_str());
} EXEC_ACTION_END;
