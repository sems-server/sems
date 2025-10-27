# Requires CMake 3.17+ with FindPython3
if(NOT Python3_FOUND)
  find_package(
    Python3
    COMPONENTS Interpreter Development
    QUIET)
endif()

if(Python3_FOUND)

  get_filename_component(PYTHON_LD_PATH ${Python3_LIBRARIES} PATH)

  find_path(SIP_INCLUDE_DIR sip.h PATHS ${Python3_INCLUDE_DIRS})
  find_program(SIP_BINARY python3-sip)

  if(SIP_INCLUDE_DIR AND SIP_BINARY)
    set(SIP_FOUND TRUE)
  endif(SIP_INCLUDE_DIR AND SIP_BINARY)

  if(SIP_FOUND)
    if(NOT SIP_FIND_QUIETLY)
      message(STATUS "Found sip includes:	${SIP_INCLUDE_DIR}/sip.h")
      message(STATUS "Found sip binary:	${SIP_BINARY}")
    endif(NOT SIP_FIND_QUIETLY)
  else(SIP_FOUND)
    if(SIP_FIND_REQUIRED)
      message(FATAL_ERROR "Could NOT find sip development files")
    endif(SIP_FIND_REQUIRED)
  endif(SIP_FOUND)

endif(Python3_FOUND)
