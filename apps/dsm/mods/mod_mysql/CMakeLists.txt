set(mod_mysql_SRCS ModMysql.cpp)

include_directories(${MYSQLCPPCONN_INCLUDE_DIR})
set(sems_dsm_module_name mod_mysql)
set(sems_dsm_module_libs ${MYSQLCPPCONN_LIBRARIES})
include(${CMAKE_SOURCE_DIR}/cmake/dsm.lib.rules.txt)
