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

SET(CMAKE_VERBOSE_MAKEFILE ON)

# Add your system-dependent network libs here. These are
# only used to build the tests (your application will need them too).
# Linux: none
# Solaris: -lsocket -lnsl

add_library(xmlrpcpp STATIC ${xmlrpcpp_SRCS})
set_target_properties(xmlrpcpp PROPERTIES OUTPUT_NAME "xmlrpcpp")
set_target_properties(xmlrpcpp PROPERTIES PREFIX "")
target_link_libraries(xmlrpcpp ${CMAKE_DL_LIBS} pthread m)

