find_path(CODEC2_INCLUDE_DIR codec2/codec2.h)
find_library(CODEC2_LIBRARIES NAMES codec2)

if(CODEC2_INCLUDE_DIR AND CODEC2_LIBRARIES)
  set(CODEC2_FOUND YES)
endif(CODEC2_INCLUDE_DIR AND CODEC2_LIBRARIES)

if(CODEC2_FOUND)
  if(NOT Codec2_FIND_QUIETLY)
    message(
      STATUS "Found codec2 includes:	${CODEC2_INCLUDE_DIR}/codec2/codec2.h")
    message(STATUS "Found codec2 library: ${CODEC2_LIBRARIES}")
  endif(NOT Codec2_FIND_QUIETLY)
else(CODEC2_FOUND)
  if(Codec2_FIND_REQUIRED)
    message(FATAL_ERROR "Could NOT find codec2 development files")
  endif(Codec2_FIND_REQUIRED)
endif(CODEC2_FOUND)
