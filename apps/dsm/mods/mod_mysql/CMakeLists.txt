set (mod_mysql_SRCS
ModMysql.cpp
)

INCLUDE_DIRECTORIES(${MYSQLCPPCONN_INCLUDE_DIR})
SET(sems_dsm_module_name mod_mysql)
SET(sems_dsm_module_libs ${MYSQLCPPCONN_LIBRARIES})
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/dsm.lib.rules.txt)
