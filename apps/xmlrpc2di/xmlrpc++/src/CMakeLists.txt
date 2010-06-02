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

# FIXME
IF(SEMS_WORKAROUND_FOR_OLD_CMAKE_2_4_FOR_STATIC_LIBS)
ADD_CUSTOM_COMMAND(TARGET xmlrpcpp POST_BUILD COMMAND cp ARGS libxmlrpcpp.a liblibxmlrpcpp.a)
ENDIF(SEMS_WORKAROUND_FOR_OLD_CMAKE_2_4_FOR_STATIC_LIBS)
