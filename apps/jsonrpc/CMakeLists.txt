set (jsonrpc_SRCS
JsonRPC.cpp
JsonRPCServer.cpp
RpcPeer.cpp
RpcServerLoop.cpp
RpcServerThread.cpp
)

INCLUDE_DIRECTORIES(${LIBEV_INCLUDE_DIR})
SET(sems_module_name jsonrpc)
SET(sems_module_libs ${sems_module_libs} ${LIBEV_LIBRARIES})
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
