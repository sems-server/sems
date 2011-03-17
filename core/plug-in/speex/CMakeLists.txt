set (speex_SRCS
 speex.c
)

INCLUDE_DIRECTORIES(${SPEEX_INCLUDE_DIR})

# module_cflags  = -ansi # -DNOFPU

add_library(speex SHARED IMPORTED)
set_property(TARGET speex PROPERTY IMPORTED_LOCATION ${SPEEX_LIBRARIES})

SET(sems_module_name speex)
SET(sems_module_libs speex)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
