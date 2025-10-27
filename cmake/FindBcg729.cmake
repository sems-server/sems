find_path(BCG729_INCLUDE_DIR bcg729/decoder.h)
find_library(BCG729_LIBRARIES NAMES bcg729)

if(BCG729_INCLUDE_DIR AND BCG729_LIBRARIES)
  set(BCG729_FOUND TRUE)
endif(BCG729_INCLUDE_DIR AND BCG729_LIBRARIES)

if(BCG729_FOUND)
  if(NOT Bcg729_FIND_QUIETLY)
    message(
      STATUS "Found bcg729 includes:	${BCG729_INCLUDE_DIR}/bcg729/decoder.h")
    message(STATUS "Found bcg729 library: ${BCG729_LIBRARIES}")
  endif(NOT Bcg729_FIND_QUIETLY)
else(BCG729_FOUND)
  if(Bcg729_FIND_REQUIRED)
    message(FATAL_ERROR "Could NOT find bcg729 development files")
  endif(Bcg729_FIND_REQUIRED)
endif(BCG729_FOUND)
