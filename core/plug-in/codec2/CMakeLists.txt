########################################################################
# SEMS Codec2
########################################################################

set(codec2_SRCS
    codec2.c
)

# Do not confuset module name and module libs they boath have the
# same name codec2
SET(sems_module_name codec2)
SET(sems_module_libs codec2)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)

