set (g722_SRCS
g722.c
)

#module_cflags  = -ansi # -DNOFPU

SET(sems_module_name g722)
SET(sems_module_libs spandsp)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
