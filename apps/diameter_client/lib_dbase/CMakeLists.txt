set (libdbase_SRCS
avp.c
diameter_msg.c
tcp_comm.c
)

SET(CMAKE_VERBOSE_MAKEFILE ON)

IF(OPENSSL_FOUND)
	ADD_DEFINITIONS(-DWITH_OPENSSL)
ENDIF(OPENSSL_FOUND)

add_library(libdbase STATIC ${libdbase_SRCS})
set_target_properties(libdbase PROPERTIES OUTPUT_NAME "libdbase")
target_link_libraries(libdbase ${CMAKE_DL_LIBS} pthread m)

