set (xmlrpc2di_SRCS
MultithreadXmlRpcServer.cpp
TOXmlRpcClient.cpp
XMLRPC2DI.cpp
xmlrpc++/src/XmlRpcClient.cpp
xmlrpc++/src/XmlRpcDispatch.cpp
xmlrpc++/src/XmlRpcMutex.cpp
xmlrpc++/src/XmlRpcServer.cpp
xmlrpc++/src/XmlRpcServerConnection.cpp
xmlrpc++/src/XmlRpcServerMethod.cpp
xmlrpc++/src/XmlRpcSocket.cpp
xmlrpc++/src/XmlRpcSource.cpp
xmlrpc++/src/XmlRpcThread.cpp
xmlrpc++/src/XmlRpcThreadedServer.cpp
xmlrpc++/src/XmlRpcUtil.cpp
xmlrpc++/src/XmlRpcValue.cpp
)

INCLUDE_DIRECTORIES (xmlrpc++/src)

SET(sems_module_name xmlrpc2di)

IF(OPENSSL_FOUND)
	ADD_DEFINITIONS(-DHAVE_XMLRPCPP_SSL -DXMLRPCPP_SUPPORT_STRUCT_ACCESS)
	INCLUDE_DIRECTORIES(${OPENSSL_INCLUDE_DIR})
	SET(sems_module_libs ${sems_module_libs} ${OPENSSL_LIBRARIES})
ENDIF(OPENSSL_FOUND)

INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
