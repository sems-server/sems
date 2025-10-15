FILE (GLOB gateway_SRCS "*.cpp")
INCLUDE_DIRECTORIES(${MISDN_INCLUDE_DIR}/mISDNuser)

SET(sems_module_name gateway)
SET(sems_module_libs ${MISDN_LIBRARIES} isdnnet)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
