set (diameter_client_SRCS
DiameterClient.cpp
ServerConnection.cpp
)

ADD_SUBDIRECTORY (lib_dbase)
INCLUDE_DIRECTORIES (lib_dbase)

set_property(TARGET libdbase PROPERTY IMPORTED_LOCATION ./lib_dbase/libdbase.a)

add_library(diameter_client SHARED ${diameter_client_SRCS})
set_target_properties(diameter_client PROPERTIES OUTPUT_NAME "diameter_client")
set_target_properties(diameter_client PROPERTIES PREFIX "")
target_link_libraries(diameter_client ${CMAKE_DL_LIBS} pthread m libdbase)

IF(OPENSSL_FOUND)
IF(CMAKE_COMPILER_IS_GNUCC)
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DWITH_OPENSSL")
ENDIF(CMAKE_COMPILER_IS_GNUCC)

IF(CMAKE_COMPILER_IS_GNUCXX)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWITH_OPENSSL")
ENDIF(CMAKE_COMPILER_IS_GNUCXX)
SET_TARGET_PROPERTIES(diameter_client PROPERTIES LINK_FLAGS "${LINK_FLAGS} -lssl -lcrypto")
ENDIF(OPENSSL_FOUND)
