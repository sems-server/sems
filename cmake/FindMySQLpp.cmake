find_path(MYSQLPP_INCLUDE_DIR mysql++/mysql++.h)
find_library(MYSQLPP_LIBRARIES NAMES mysqlpp)

if(MYSQLPP_INCLUDE_DIR AND MYSQLPP_LIBRARIES)
  set(MYSQLPP_FOUND TRUE)
endif(MYSQLPP_INCLUDE_DIR AND MYSQLPP_LIBRARIES)

if(MYSQLPP_FOUND)
  if(NOT MySQLpp_FIND_QUIETLY)
    message(
      STATUS "Found MySQL++ includes:	${MYSQLPP_INCLUDE_DIR}/mysql++/mysql++.h")
    message(STATUS "Found MySQL++ library: ${MYSQLPP_LIBRARIES}")
  endif(NOT MySQLpp_FIND_QUIETLY)
else(MYSQLPP_FOUND)
  if(MySQLpp_FIND_REQUIRED)
    message(FATAL_ERROR "Could NOT find MySQL++ development files")
  endif(MySQLpp_FIND_REQUIRED)
endif(MYSQLPP_FOUND)

# Check for buried mysql_version.h
find_path(MYSQL_INCLUDE_MYSQL_VERSION_H mysql_version.h)
if(NOT MYSQL_INCLUDE_MYSQL_VERSION_H)
  find_path(MYSQL_INCLUDE_BURIED_MYSQL_VERSION_H mysql/mysql_version.h)
  if(MYSQL_INCLUDE_BURIED_MYSQL_VERSION_H)
    add_definitions(-DMYSQLPP_MYSQL_HEADERS_BURIED)
  endif(MYSQL_INCLUDE_BURIED_MYSQL_VERSION_H)
endif(NOT MYSQL_INCLUDE_MYSQL_VERSION_H)
