find_path(LIBZRTP_INCLUDE_DIR libzrtp/zrtp.h)
find_library(LIBZRTP_LIBRARIES NAMES zrtp)

# bnlib (Colin Plumb's big-number library) is sometimes installed as a
# separate archive alongside libzrtp.  Append it when present.
find_library(LIBBN_LIBRARY NAMES bn)

if(LIBZRTP_INCLUDE_DIR AND LIBZRTP_LIBRARIES)
  set(LIBZRTP_FOUND TRUE)
endif(LIBZRTP_INCLUDE_DIR AND LIBZRTP_LIBRARIES)

if(LIBZRTP_FOUND)
  if(LIBBN_LIBRARY)
    list(APPEND LIBZRTP_LIBRARIES ${LIBBN_LIBRARY})
  endif(LIBBN_LIBRARY)
  if(NOT Libzrtp_FIND_QUIETLY)
    message(STATUS "Found libzrtp includes:	${LIBZRTP_INCLUDE_DIR}/libzrtp/zrtp.h")
    message(STATUS "Found libzrtp library: ${LIBZRTP_LIBRARIES}")
  endif(NOT Libzrtp_FIND_QUIETLY)
else(LIBZRTP_FOUND)
  if(Libzrtp_FIND_REQUIRED)
    message(FATAL_ERROR "Could NOT find libzrtp development files")
  endif(Libzrtp_FIND_REQUIRED)
endif(LIBZRTP_FOUND)
