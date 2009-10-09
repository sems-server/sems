set (diameter_client_SRCS
DiameterClient.cpp
ServerConnection.cpp
)

ADD_SUBDIRECTORY (lib_dbase)
INCLUDE_DIRECTORIES (lib_dbase)

SET_PROPERTY(TARGET libdbase PROPERTY IMPORTED_LOCATION ./lib_dbase/libdbase.a)

SET(sems_module_name diameter_client)
SET(sems_module_libs libdbase)

IF(OPENSSL_FOUND)
	ADD_DEFINITIONS(-DWITH_OPENSSL)
	SET(sems_module_libs ${sems_module_libs} ssl crypto)
ENDIF(OPENSSL_FOUND)

INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
