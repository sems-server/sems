set (xmlrpc2di_SRCS
MultithreadXmlRpcServer.cpp
TOXmlRpcClient.cpp
XMLRPC2DI.cpp
)

ADD_SUBDIRECTORY (xmlrpc++)
INCLUDE_DIRECTORIES (xmlrpc++/src)

SET(sems_module_name xmlrpc2di)
SET(sems_module_libs xmlrpcpp)

IF(OPENSSL_FOUND)
	ADD_DEFINITIONS(-DHAVE_XMLRPCPP_SSL -DXMLRPCPP_SUPPORT_STRUCT_ACCESS)
	SET(sems_module_libs ${sems_module_libs} ssl)
ENDIF(OPENSSL_FOUND)

INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/config.rules.txt)
