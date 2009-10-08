set (early_announce_SRCS
EarlyAnnounce.cpp
)

ADD_LIBRARY(early_announce SHARED ${early_announce_SRCS})
TARGET_LINK_LIBRARIES(early_announce ${CMAKE_DL_LIBS})

IF(MYSQLPP_FOUND)
	INCLUDE_DIRECTORIES(/usr/include/mysql)
	INCLUDE_DIRECTORIES(${MYSQLPP_INCLUDE_DIR}/mysql++)
	ADD_DEFINITIONS(-DUSE_MYSQL)
	TARGET_LINK_LIBRARIES(early_announce ${CMAKE_DL_LIBS} mysqlpp)
ENDIF(MYSQLPP_FOUND)
