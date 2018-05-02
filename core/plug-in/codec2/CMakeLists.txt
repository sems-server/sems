########################################################################
# SEMS Plugin for CODEC2
# http://www.rowetel.com/?page_id=452
########################################################################

set(codec2_SRCS
    codec2.c
)

# Do not confuse module name and module libs, they both have the
# same name codec2
SET(sems_module_name codec2)
SET(sems_module_libs codec2)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
