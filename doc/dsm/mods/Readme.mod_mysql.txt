mod_mysql - Copyright (C) 2009 TelTech Systems Inc.

configuration
=============

DB connection can be configured in dsm.conf: 
$config.db_url : mysql://user:pwd@host/db

Actions:
=======
-- connect connection
 
 mysql.connect([db_url])
  - sets $errno if error occured (arg,) and
   $db.ereason
   $db.errno if available


-- disconnect connection
 mysql.disconnect()
  - sets $errno if error occured and
   $db.ereason

-- execute query 

 mysql.execute(INSERT INTO mytable (username,pin) values ($username, $pin));

 * query string has replacement for vars and params, \$=$, \#=#
 * sets 
    $errno
    $db.rows
    $db.info
    $db.insert_id

 
-- execute query and store result
 mysql.query(SELECT * FROM mytable);

 * query string has replacement for vars and params, \$=$, \#=#
    mysql.query(SELECT * FROM mytable where user=$username);
    mysql.query(SELECT * FROM mytable where key=#key);

 * sets 
    $errno
    $db.rows

-- execute query and get first or nth result row
 mysql.queryGetResult(SELECT * FROM mytable [, row]);

 * query string has replacement for vars and params, \$=$, \#=#
    mysql.query(SELECT * FROM mytable where user=$username);
    mysql.query(SELECT * FROM mytable where key=#key);

 * sets 
    $errno
    $db.rows

-- resolve query parameters (as it would do on mysql.execute, mysql.query etc)
   mysql.resolveQueryParams(str)

 * query string has replacement for vars and params, \$=$, \#=#
    mysql.resolveQueryParams(SELECT * FROM mytable where user=$username);

 * sets 
    $db.qstr

-- get result in $var
 mysql.getResult([rowindex[, colname]])

 get first result row in $vars, if any
   mysql.getResult()

 get nth result row
   mysql.getResult(2) 

 get nth result row, only named column
   mysql.getResult(2, colname)

-- gets client version into $db.client_version
 mysql.getClientVersion()

-- save result into another name
  mysql.saveResult(string name)

-- use saved result
  mysql.useResult(string name)

-- play a file from DB 
  mysql.playDBAudio(string query, string filename)

  filename is there to detect file type

-- play a file from DB, looped 
  mysql.playDBAudioLooped(string query, string filename)

  filename is there to detect file type

-- play a file from DB, at the front in the playlist 
  mysql.playDBAudioFront(string query, string filename)

  filename is there to detect file type

-- get a file from DB to local fs
  mysql.getFileFromDB(string query, string filename)

-- put a local file into DB
  mysql.putFileToDB(string query, string filename)

  __FILE__ in query is replaced with the contents of the file at 'filename'

  sets $db.rows, $db.info, $db.insert_id

 -- escape:
 mysql.escape($dstvar=$src);

  save SQL-escaped version of $src in $dstvar, taking into account default
  character set of connected DB server. A connection to MySQL server must be
  established!

  examples:
     mysql.escape($safe_user=@user);


Conditions
==========
  mysql.hasResult()

  mysql.connected()

ERROR codes 
===========
$errno:

#define DSM_ERRNO_MY_CONNECTION "connection"
#define DSM_ERRNO_MY_QUERY      "query"
#define DSM_ERRNO_MY_NORESULT   "result"
#define DSM_ERRNO_MY_NOROW      "result"
#define DSM_ERRNO_MY_NOCOLUMN   "result"

Internals
=========
mysql++ objects are saved to AmArg objects, and ownership is transferred
to the session:

 MySQL++DB objects   saved to 
 ------------------+----------------
  Connection       | avar["db.con"]
                   | 
  Result           | avar["db.res"]
                   | 
  Result           | avar[parameter] on mysql.saveResult

Note: Due to the implementation of MySQL++, the complete result set has 
to be copied one more time internally, if mysql.query() is used. So if
only the first (or only) row is of interest, use mysql.queryGetResult() 


