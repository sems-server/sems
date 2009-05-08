set (early_announce_SRCS
EarlyAnnounce.cpp
)

add_library(early_announce SHARED ${early_announce_SRCS})
set_target_properties(early_announce PROPERTIES OUTPUT_NAME "early_announce")
set_target_properties(early_announce PROPERTIES PREFIX "")
target_link_libraries(early_announce ${CMAKE_DL_LIBS} pthread m)

IF(MYSQLPP_FOUND)
IF(CMAKE_COMPILER_IS_GNUCC)
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_MYSQL -I/usr/include/mysql++ -I/usr/include/mysql")
ENDIF(CMAKE_COMPILER_IS_GNUCC)

IF(CMAKE_COMPILER_IS_GNUCXX)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_MYSQL -I/usr/include/mysql++ -I/usr/include/mysql")
ENDIF(CMAKE_COMPILER_IS_GNUCXX)
SET_TARGET_PROPERTIES(early_announce PROPERTIES LINK_FLAGS "${LINK_FLAGS} -lmysqlpp")
ENDIF(MYSQLPP_FOUND)
