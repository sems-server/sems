mod_mysql - Copyright (C) 2009 TelTech Systems Inc.

configuration
=============

DB connection can be configured in dsm.conf: 
$config.db_url : mysql://user:pwd@host/db

Actions:
=======
-- connect connection
 

 mysql.connect([db_url])
  - sets $errno if error occured and
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
    $db.success
    $db.rows
    $db.info
    $db.insert_id

 
-- execute query and store result
 mysql.query(SELECT * FROM mytable);

 * query string has replacement for vars and params, \$=$, \#=#
    mysql.query(SELECT * FROM mytable where user=$username);
    mysql.query(SELECT * FROM mytable where key=#key);

 * sets 
    $db.success
    $db.rows

-- execute query and get first or nth result row
 mysql.queryGetResult(SELECT * FROM mytable [, row]);

 * query string has replacement for vars and params, \$=$, \#=#
    mysql.query(SELECT * FROM mytable where user=$username);
    mysql.query(SELECT * FROM mytable where key=#key);

 * sets 
    $db.success
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



Conditions
==========
  mysql.hasResult()

  mysql.connected()

ERROR codes 
===========
$errno:
#define DSM_ERRNO_MY_CONNECTION "30"
#define DSM_ERRNO_MY_QUERY      "31"
#define DSM_ERRNO_MY_NORESULT   "32"
#define DSM_ERRNO_MY_NOROW      "33"
#define DSM_ERRNO_MY_NOCOLUMN   "34"

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

Note: Due to the implementation of MySQL++, the complete result set has 
to be copied one more time internally, if mysql.query() is used. So if
only the first (or only) row is of interest, use mysql.queryGetResult() 


