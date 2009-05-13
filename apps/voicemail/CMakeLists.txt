set (voicemail_SRCS
AmMail.cpp
AmSmtpClient.cpp
AnswerMachine.cpp
EmailTemplate.cpp
)

ADD_LIBRARY(voicemail SHARED ${voicemail_SRCS})
SET_TARGET_PROPERTIES(voicemail PROPERTIES OUTPUT_NAME "voicemail")
SET_TARGET_PROPERTIES(voicemail PROPERTIES PREFIX "")
TARGET_LINK_LIBRARIES(voicemail ${CMAKE_DL_LIBS} pthread m)

# Currently cannot be build with mysql++
#IF(MYSQLPP_FOUND)
#	IF(CMAKE_COMPILER_IS_GNUCC)
#		SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_MYSQL -I/usr/include/mysql++ -I/usr/include/mysql")
#	ENDIF(CMAKE_COMPILER_IS_GNUCC)
#	IF(CMAKE_COMPILER_IS_GNUCXX)
#		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_MYSQL -I/usr/include/mysql++ -I/usr/include/mysql")
#	ENDIF(CMAKE_COMPILER_IS_GNUCXX)
#	TARGET_LINK_LIBRARIES(voicemail ${CMAKE_DL_LIBS} mysqlpp)
#ENDIF(MYSQLPP_FOUND)
