set (opus_SRCS
opus.c
)

#module_cflags  = -ansi # -DNOFPU

SET(sems_module_name opus)
SET(sems_module_libs opus)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
