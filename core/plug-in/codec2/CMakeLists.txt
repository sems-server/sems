# ##############################################################################
# SEMS Plugin for CODEC2 http://www.rowetel.com/?page_id=452
# ##############################################################################

set(codec2_SRCS codec2.c)

set(sems_module_name codec2)
set(sems_module_libs ${CODEC2_LIBRARIES})
include(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
