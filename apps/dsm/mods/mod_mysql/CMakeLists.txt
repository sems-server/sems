set (mod_mysql_SRCS
ModMysql.cpp
)

IF(CMAKE_COMPILER_IS_GNUCC)
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I/usr/include/mysql++ -I/usr/include/mysql")
ENDIF(CMAKE_COMPILER_IS_GNUCC)

IF(CMAKE_COMPILER_IS_GNUCXX)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I/usr/include/mysql++ -I/usr/include/mysql")
ENDIF(CMAKE_COMPILER_IS_GNUCXX)

add_library(mod_mysql SHARED ${mod_mysql_SRCS})
set_target_properties(mod_mysql PROPERTIES OUTPUT_NAME "mod_mysql")
set_target_properties(mod_mysql PROPERTIES PREFIX "")
target_link_libraries(mod_mysql ${CMAKE_DL_LIBS} pthread m mysqlpp)

