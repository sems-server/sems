find_path(LIBZRTP_INCLUDE_DIR
    NAMES zrtp
    PATHS /usr/local/include /usr/include
)

find_library(LIBZRTP_LIBRARY
  NAME zrtp
  PATHS /usr/local/lib /usr/lib
)

find_library(BN_LIBRARY
  NAME bn
  PATHS /usr/local/lib /usr/lib
)


if(LIBZRTP_INCLUDE_DIR AND LIBZRTP_LIBRARY AND BN_LIBRARY)
  set(LIBZRTP_FOUND TRUE)
endif()

if(LIBZRTP_FOUND)
  set( LIBZRTP_LIBRARIES ${LIBZRTP_LIBRARY} ${BN_LIBRARY} )
  if(NOT Libzrtp_FIND_QUIETLY)
    message(STATUS "Found libzrtp includes:	${LIBZRTP_INCLUDE_DIR}/zrtp/zrtp.h")
    message(STATUS "Found libzrtp library: ${LIBZRTP_LIBRARIES}")
  endif(NOT Libzrtp_FIND_QUIETLY)
else(LIBZRTP_FOUND)
  if(Libzrtp_FIND_REQUIRED)
    message(FATAL_ERROR "Could NOT find libzrtp development files")
  endif(Libzrtp_FIND_REQUIRED)
endif(LIBZRTP_FOUND)
