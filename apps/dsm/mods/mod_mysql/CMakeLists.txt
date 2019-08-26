set (mod_mysql_SRCS
ModMysql.cpp
)

INCLUDE_DIRECTORIES(/usr/include/mysql)
INCLUDE_DIRECTORIES(/usr/include/cppcon)

SET(sems_dsm_module_name mod_mysql)
SET(sems_dsm_module_libs mysqlcppconn)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/dsm.lib.rules.txt)
