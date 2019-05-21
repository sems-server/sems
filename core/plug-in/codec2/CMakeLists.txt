########################################################################
# SEMS Plugin for CODEC2
# http://www.rowetel.com/?page_id=452
########################################################################

set(codec2_SRCS
    codec2.c
)

SET(sems_module_name codec2)
SET(sems_module_libs ${CODEC2_LIBRARIES})
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
