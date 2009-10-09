set (mod_mysql_SRCS
ModMysql.cpp
)

INCLUDE_DIRECTORIES(/usr/include/mysql)
INCLUDE_DIRECTORIES(${MYSQLPP_INCLUDE_DIR}/mysql++)

SET(sems_dsm_module_name mod_mysql)
SET(sems_dsm_module_libs mysqlpp)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/dsm.lib.rules.txt)

