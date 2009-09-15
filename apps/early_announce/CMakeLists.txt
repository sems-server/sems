set (early_announce_SRCS
EarlyAnnounce.cpp
)

ADD_LIBRARY(early_announce SHARED ${early_announce_SRCS})
SET_TARGET_PROPERTIES(early_announce PROPERTIES OUTPUT_NAME "early_announce")
TARGET_LINK_LIBRARIES(early_announce ${CMAKE_DL_LIBS} pthread m)

IF(MYSQLPP_FOUND)
	IF(CMAKE_COMPILER_IS_GNUCC)
		SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_MYSQL -I/usr/include/mysql++ -I/usr/include/mysql")
	ENDIF(CMAKE_COMPILER_IS_GNUCC)

	IF(CMAKE_COMPILER_IS_GNUCXX)
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_MYSQL -I/usr/include/mysql++ -I/usr/include/mysql")
	ENDIF(CMAKE_COMPILER_IS_GNUCXX)
	TARGET_LINK_LIBRARIES(early_announce ${CMAKE_DL_LIBS} mysqlpp)
ENDIF(MYSQLPP_FOUND)
