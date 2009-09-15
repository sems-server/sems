set (mod_mysql_SRCS
ModMysql.cpp
)

INCLUDE_DIRECTORIES(/usr/include/mysql)
INCLUDE_DIRECTORIES(${MYSQLPP_INCLUDE_DIR}/mysql++)

ADD_LIBRARY(mod_mysql SHARED ${mod_mysql_SRCS})
SET_TARGET_PROPERTIES(mod_mysql PROPERTIES OUTPUT_NAME "mod_mysql")
TARGET_LINK_LIBRARIES(mod_mysql ${CMAKE_DL_LIBS} pthread m mysqlpp)

