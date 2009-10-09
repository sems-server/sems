set (speex_SRCS
 speex.c
)

INCLUDE_DIRECTORIES(${SPEEX_INCLUDE_DIR})

# module_cflags  = -ansi # -DNOFPU

SET(sems_module_name speex)
SET(sems_module_libs speex)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
