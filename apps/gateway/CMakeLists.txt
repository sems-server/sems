file(GLOB gateway_SRCS "*.cpp")

add_definitions(-DGW_VERSION="0.1")
include_directories(${MISDN_INCLUDE_DIR}/mISDNuser)

set(sems_module_name gateway)
set(sems_module_libs ${MISDN_LIBRARIES} isdnnet)
include(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
