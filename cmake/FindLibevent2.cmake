if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
  find_path(LIBEVENT2_INCLUDE_DIR event2/event.h HINTS /usr/local/include)
  set(CMAKE_SHARED_LIBRARY_PREFIX ${CMAKE_SHARED_LIBRARY_PREFIX} /usr/local/lib)
else(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
  find_path(LIBEVENT2_INCLUDE_DIR event2/event.h HINTS /usr/include/event2)
endif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
find_library(LIBEVENT2_LIBRARIES NAMES event)

if(LIBEVENT2_INCLUDE_DIR AND LIBEVENT2_LIBRARIES)
  set(LIBEVENT2_FOUND TRUE)
endif(LIBEVENT2_INCLUDE_DIR AND LIBEVENT2_LIBRARIES)

if(LIBEVENT2_FOUND)
  if(NOT Libevent2_FIND_QUIETLY)
    message(
      STATUS "Found libevent2 includes: ${LIBEVENT2_INCLUDE_DIR}/event2/event.h"
    )
    message(STATUS "Found libevent2 library: ${LIBEVENT2_LIBRARIES}")
  endif(NOT Libevent2_FIND_QUIETLY)
else(LIBEVENT2_FOUND)
  if(Libevent2_FIND_REQUIRED)
    message(FATAL_ERROR "Could NOT find libevent2 development files")
  endif(Libevent2_FIND_REQUIRED)
endif(LIBEVENT2_FOUND)
