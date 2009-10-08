set (xmlrpcpp_SRCS
XmlRpcClient.cpp
XmlRpcDispatch.cpp
XmlRpcMutex.cpp
XmlRpcServer.cpp
XmlRpcServerConnection.cpp
XmlRpcServerMethod.cpp
XmlRpcSocket.cpp
XmlRpcSource.cpp
XmlRpcThread.cpp
XmlRpcThreadedServer.cpp
XmlRpcUtil.cpp
XmlRpcValue.cpp
)

# Add your system-dependent network libs here. These are
# only used to build the tests (your application will need them too).
# Linux: none
# Solaris: -lsocket -lnsl

ADD_LIBRARY(xmlrpcpp STATIC ${xmlrpcpp_SRCS})
TARGET_LINK_LIBRARIES(xmlrpcpp ${CMAKE_DL_LIBS})

