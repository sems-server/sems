set (voicemail_SRCS
AmMail.cpp
AmSmtpClient.cpp
AnswerMachine.cpp
EmailTemplate.cpp
)

set (voicemail_audio_files
beep.wav
default_en.wav
)

# Currently cannot be build with mysql++
#IF(MYSQLPP_FOUND)
#	IF(CMAKE_COMPILER_IS_GNUCC)
#		SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_MYSQL -I/usr/include/mysql++ -I/usr/include/mysql")
#	ENDIF(CMAKE_COMPILER_IS_GNUCC)
#	IF(CMAKE_COMPILER_IS_GNUCXX)
#		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_MYSQL -I/usr/include/mysql++ -I/usr/include/mysql")
#	ENDIF(CMAKE_COMPILER_IS_GNUCXX)
#	SET(sems_module_libs mysqlpp)
#ENDIF(MYSQLPP_FOUND)

SET(sems_module_name voicemail)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
INSTALL(
	FILES ./default.template  DESTINATION ${SEMS_CFG_PREFIX}/etc/sems/
)
