find_path(LIBSAMPLERATE_INCLUDE_DIR samplerate.h)
find_library(LIBSAMPLERATE_LIBRARIES NAMES samplerate)

if(LIBSAMPLERATE_INCLUDE_DIR AND LIBSAMPLERATE_LIBRARIES)
  set(LIBSAMPLERATE_FOUND TRUE)
endif(LIBSAMPLERATE_INCLUDE_DIR AND LIBSAMPLERATE_LIBRARIES)

if(LIBSAMPLERATE_FOUND)
  if(NOT Libsamplerate_FIND_QUIETLY)
    message(
      STATUS
        "Found libsamplerate includes:	${LIBSAMPLERATE_INCLUDE_DIR}/samplerate.h"
    )
    message(STATUS "Found libsamplerate library: ${LIBSAMPLERATE_LIBRARIES}")
  endif(NOT Libsamplerate_FIND_QUIETLY)
else(LIBSAMPLERATE_FOUND)
  if(Libsamplerate_FIND_REQUIRED)
    message(FATAL_ERROR "Could NOT find libsamplerate development files")
  endif(Libsamplerate_FIND_REQUIRED)
endif(LIBSAMPLERATE_FOUND)
