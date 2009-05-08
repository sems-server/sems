set (gateway_SRCS
GWSession.cpp
GatewayFactory.cpp
mISDNChannel.cpp
mISDNNames.cpp
mISDNStack.cpp
)

IF(CMAKE_COMPILER_IS_GNUCC)
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DGW_VERSION='\"0.1\"'")
ENDIF(CMAKE_COMPILER_IS_GNUCC)

IF(CMAKE_COMPILER_IS_GNUCXX)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGW_VERSION='\"0.1\"'")
ENDIF(CMAKE_COMPILER_IS_GNUCXX)

add_library(gateway SHARED ${gateway_SRCS})
set_target_properties(gateway PROPERTIES OUTPUT_NAME "gateway")
set_target_properties(gateway PROPERTIES PREFIX "")
target_link_libraries(gateway ${CMAKE_DL_LIBS} pthread m mISDN isdnnet)

