set (diameter_client_SRCS
DiameterClient.cpp
ServerConnection.cpp
)

ADD_SUBDIRECTORY (lib_dbase)
INCLUDE_DIRECTORIES (lib_dbase)

SET_PROPERTY(TARGET libdbase PROPERTY IMPORTED_LOCATION ./lib_dbase/libdbase.a)

ADD_LIBRARY(diameter_client SHARED ${diameter_client_SRCS})
SET_TARGET_PROPERTIES(diameter_client PROPERTIES OUTPUT_NAME "diameter_client")
TARGET_LINK_LIBRARIES(diameter_client ${CMAKE_DL_LIBS} pthread m libdbase)

IF(OPENSSL_FOUND)
	ADD_DEFINITIONS(-DWITH_OPENSSL)
	TARGET_LINK_LIBRARIES(diameter_client ${CMAKE_DL_LIBS} ssl crypto)
ENDIF(OPENSSL_FOUND)
