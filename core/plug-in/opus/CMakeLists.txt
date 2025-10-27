set(opus_SRCS opus.c)

# module_cflags  = -ansi # -DNOFPU

set(sems_module_name opus)
set(sems_module_libs ${OPUS_LIBRARIES})
include(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
