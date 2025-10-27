set(xmlrpc2di_SRCS
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
    xmlrpc++/src/XmlRpcValue.cpp)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  find_path(OPENSSL_INCLUDE_DIR openssl/crypto.h
            HINTS /usr/local/Cellar/openssl@3/3.4.1/include)
  set(OPENSSL_FOUND TRUE)
endif()

include_directories(xmlrpc++/src)

set(sems_module_name xmlrpc2di)

if(OPENSSL_FOUND)
  add_definitions(-DHAVE_XMLRPCPP_SSL -DXMLRPCPP_SUPPORT_STRUCT_ACCESS)
  include_directories(${OPENSSL_INCLUDE_DIR})
  set(sems_module_libs ${sems_module_libs} ${OPENSSL_LIBRARIES})
endif(OPENSSL_FOUND)

include(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
