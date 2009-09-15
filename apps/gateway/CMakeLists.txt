set (gateway_SRCS
GWSession.cpp
GatewayFactory.cpp
mISDNChannel.cpp
mISDNNames.cpp
mISDNStack.cpp
)

ADD_DEFINITIONS(-DGW_VERSION="0.1")

ADD_LIBRARY(gateway SHARED ${gateway_SRCS})
SET_TARGET_PROPERTIES(gateway PROPERTIES OUTPUT_NAME "gateway")
TARGET_LINK_LIBRARIES(gateway ${CMAKE_DL_LIBS} pthread m mISDN isdnnet)

