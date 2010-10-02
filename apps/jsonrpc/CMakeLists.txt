set (jsonrpc_SRCS
JsonRPC.cpp
JsonRPCServer.cpp
RpcPeer.cpp
RpcServerLoop.cpp
RpcServerThread.cpp
)

SET(sems_module_name jsonrpc)
SET(sems_module_libs ${sems_module_libs} ev)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/config.rules.txt)
