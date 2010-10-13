set (early_announce_SRCS
EarlyAnnounce.cpp
)

IF(MYSQLPP_FOUND)
	INCLUDE_DIRECTORIES(/usr/include/mysql)
	INCLUDE_DIRECTORIES(${MYSQLPP_INCLUDE_DIR}/mysql++)
	ADD_DEFINITIONS(-DUSE_MYSQL)
	SET(sems_module_libs mysqlpp)
ENDIF(MYSQLPP_FOUND)

SET(sems_module_name early_announce)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
