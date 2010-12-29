set (gateway_SRCS
GWSession.cpp
GatewayFactory.cpp
mISDNChannel.cpp
mISDNNames.cpp
mISDNStack.cpp
)

ADD_DEFINITIONS(-DGW_VERSION=\\\"0.1\\\")
INCLUDE_DIRECTORIES(${MISDN_INCLUDE_DIR}/mISDNuser)

SET(sems_module_name gateway)
SET(sems_module_libs mISDN isdnnet)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
