set (early_announce_SRCS
EarlyAnnounce.cpp
)

IF(MYSQLCPPCONN_FOUND)
	INCLUDE_DIRECTORIES(/usr/include/mysql)
	INCLUDE_DIRECTORIES(${MYSQLCPPCONN_INCLUDE_DIR}/cppconn)
	ADD_DEFINITIONS(-DUSE_MYSQL)
	SET(sems_module_libs mysqlcppconn)
ENDIF(MYSQLCPPCONN_FOUND)

SET(sems_module_name early_announce)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
