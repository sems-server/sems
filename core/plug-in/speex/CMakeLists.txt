set(speex_SRCS speex.c)

include_directories(${SPEEX_INCLUDE_DIR})

# module_cflags  = -ansi # -DNOFPU

add_library(speex SHARED IMPORTED)
set_property(TARGET speex PROPERTY IMPORTED_LOCATION ${SPEEX_LIBRARIES})

set(sems_module_name speex)
set(sems_module_libs ${SPEEX_LIBRARIES})
include(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
