set (diameter_client_SRCS
DiameterClient.cpp
ServerConnection.cpp
lib_dbase/avp.c
lib_dbase/diameter_msg.c
lib_dbase/tcp_comm.c
)

INCLUDE_DIRECTORIES (lib_dbase)

SET(sems_module_name diameter_client)

IF(OPENSSL_FOUND)
	ADD_DEFINITIONS(-DWITH_OPENSSL)
	INCLUDE_DIRECTORIES(${OPENSSL_INCLUDE_DIR})
	SET(sems_module_libs ${sems_module_libs} ${OPENSSL_LIBRARIES})
ENDIF(OPENSSL_FOUND)

INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
