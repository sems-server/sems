set (binrpcctrl_SRCS
BrpcCtrlInterface.cpp
ConnPool.cpp
CtrlServer.cpp
)

ADD_SUBDIRECTORY (libbinrpc)

INCLUDE_DIRECTORIES (libbinrpc/include)

set_property(TARGET libbinrpc PROPERTY IMPORTED_LOCATION ./libbinrpc/src/binrpcctrl.a)

add_library(binrpcctrl SHARED ${binrpcctrl_SRCS})
set_target_properties(binrpcctrl PROPERTIES OUTPUT_NAME "binrpcctrl")
set_target_properties(binrpcctrl PROPERTIES PREFIX "")
target_link_libraries(binrpcctrl ${CMAKE_DL_LIBS} pthread m libbinrpc)



