set (early_announce_SRCS
EarlyAnnounce.cpp
)

ADD_LIBRARY(early_announce SHARED ${early_announce_SRCS})
SET_TARGET_PROPERTIES(early_announce PROPERTIES OUTPUT_NAME "early_announce")
TARGET_LINK_LIBRARIES(early_announce ${CMAKE_DL_LIBS} pthread m)

IF(MYSQLPP_FOUND)
	INCLUDE_DIRECTORIES(/usr/include/mysql)
	INCLUDE_DIRECTORIES(${MYSQLPP_INCLUDE_DIR}/mysql++)
	ADD_DEFINITIONS(-DUSE_MYSQL)
	TARGET_LINK_LIBRARIES(early_announce ${CMAKE_DL_LIBS} mysqlpp)
ENDIF(MYSQLPP_FOUND)
