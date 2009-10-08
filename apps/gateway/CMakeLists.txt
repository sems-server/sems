set (gateway_SRCS
GWSession.cpp
GatewayFactory.cpp
mISDNChannel.cpp
mISDNNames.cpp
mISDNStack.cpp
)

ADD_DEFINITIONS(-DGW_VERSION="0.1")
INCLUDE_DIRECTORIES(${MISDN_INCLUDE_DIR}/mISDNuser)

ADD_LIBRARY(gateway SHARED ${gateway_SRCS})
TARGET_LINK_LIBRARIES(gateway ${CMAKE_DL_LIBS} mISDN isdnnet)

