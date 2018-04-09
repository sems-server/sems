########################################################################
# SEMS Codec2
########################################################################

set(sems_codec2_SRCS
    codec2.c
)

SET(sems_module_name sems_codec2)
SET(sems_module_libs codec2)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)

