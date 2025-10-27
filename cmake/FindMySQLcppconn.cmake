find_path(MYSQLCPPCONN_INCLUDE_DIR cppconn/version_info.h
          /usr/include/mysql-cppconn/jdbc/)
find_library(MYSQLCPPCONN_LIBRARIES NAMES mysqlcppconn)

if(MYSQLCPPCONN_INCLUDE_DIR AND MYSQLCPPCONN_LIBRARIES)
  set(MYSQLCPPCONN_FOUND TRUE)
endif(MYSQLCPPCONN_INCLUDE_DIR AND MYSQLCPPCONN_LIBRARIES)

if(MYSQLCPPCONN_FOUND)
  if(NOT MySQLcppconn_FIND_QUIETLY)
    message(
      STATUS "Found MySQL-connector-C++ includes:	${MYSQLCPPCONN_INCLUDE_DIR}")
    message(
      STATUS "Found MySQL-connector-C++ library: ${MYSQLCPPCONN_LIBRARIES}")
  endif(NOT MySQLcppconn_FIND_QUIETLY)
else(MYSQLCPPCONN_FOUND)
  if(MySQLcppconn_FIND_REQUIRED)
    message(FATAL_ERROR "Could NOT find MySQL-connector-C++ development files")
  endif(MySQLcppconn_FIND_REQUIRED)
endif(MYSQLCPPCONN_FOUND)

# Check for buried mysql_version.h
find_path(MYSQL_INCLUDE_MYSQL_VERSION_H mysql_version.h)
if(NOT MYSQL_INCLUDE_MYSQL_VERSION_H)
  find_path(MYSQL_INCLUDE_BURIED_MYSQL_VERSION_H mysql/mysql_version.h)
  if(MYSQL_INCLUDE_BURIED_MYSQL_VERSION_H)
    add_definitions(-DMYSQLPP_MYSQL_HEADERS_BURIED)
  endif(MYSQL_INCLUDE_BURIED_MYSQL_VERSION_H)
endif(NOT MYSQL_INCLUDE_MYSQL_VERSION_H)
