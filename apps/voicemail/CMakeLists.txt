set (voicemail_SRCS
AmMail.cpp
AmSmtpClient.cpp
AnswerMachine.cpp
EmailTemplate.cpp
)

add_library(voicemail SHARED ${voicemail_SRCS})
set_target_properties(voicemail PROPERTIES OUTPUT_NAME "voicemail")
set_target_properties(voicemail PROPERTIES PREFIX "")
target_link_libraries(voicemail ${CMAKE_DL_LIBS} pthread m)

# Currently cannot be build with mysql++
#IF(MYSQLPP_FOUND)
#IF(CMAKE_COMPILER_IS_GNUCC)
#	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_MYSQL -I/usr/include/mysql++ -I/usr/include/mysql")
#ENDIF(CMAKE_COMPILER_IS_GNUCC)
#IF(CMAKE_COMPILER_IS_GNUCXX)
#	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_MYSQL -I/usr/include/mysql++ -I/usr/include/mysql")
#ENDIF(CMAKE_COMPILER_IS_GNUCXX)
#SET_TARGET_PROPERTIES(voicemail PROPERTIES LINK_FLAGS "${LINK_FLAGS} -lmysqlpp")
#ENDIF(MYSQLPP_FOUND)
