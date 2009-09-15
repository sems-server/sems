set (binrpcctrl_SRCS
BrpcCtrlInterface.cpp
ConnPool.cpp
CtrlServer.cpp
)

ADD_SUBDIRECTORY (libbinrpc)

INCLUDE_DIRECTORIES (libbinrpc/include)

SET_PROPERTY(TARGET libbinrpc PROPERTY IMPORTED_LOCATION ./libbinrpc/src/binrpcctrl.a)

ADD_LIBRARY(binrpcctrl SHARED ${binrpcctrl_SRCS})
SET_TARGET_PROPERTIES(binrpcctrl PROPERTIES OUTPUT_NAME "binrpcctrl")
TARGET_LINK_LIBRARIES(binrpcctrl ${CMAKE_DL_LIBS} pthread m libbinrpc)



