set (binrpcctrl_SRCS
BrpcCtrlInterface.cpp
ConnPool.cpp
CtrlServer.cpp
)

ADD_SUBDIRECTORY (libbinrpc)

INCLUDE_DIRECTORIES (libbinrpc/include)

SET_PROPERTY(TARGET libbinrpc PROPERTY IMPORTED_LOCATION ./libbinrpc/src/binrpcctrl.a)

ADD_LIBRARY(binrpcctrl SHARED ${binrpcctrl_SRCS})
TARGET_LINK_LIBRARIES(binrpcctrl ${CMAKE_DL_LIBS} libbinrpc)



