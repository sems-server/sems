set(jsonrpc_SRCS JsonRPC.cpp JsonRPCServer.cpp RpcPeer.cpp RpcServerLoop.cpp
                 RpcServerThread.cpp)

include_directories(${LIBEV_INCLUDE_DIR})
set(sems_module_name jsonrpc)
set(sems_module_libs ${sems_module_libs} ${LIBEV_LIBRARIES})
include(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
