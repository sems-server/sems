set(diameter_client_SRCS
    DiameterClient.cpp ServerConnection.cpp lib_dbase/avp.c
    lib_dbase/diameter_msg.c lib_dbase/tcp_comm.c)

include_directories(lib_dbase)

set(sems_module_name diameter_client)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  find_path(OPENSSL_INCLUDE_DIR openssl/crypto.h
            HINTS /usr/local/Cellar/openssl@3/3.4.1/include)
endif()

if(OPENSSL_FOUND)
  add_definitions(-DWITH_OPENSSL)
  include_directories(${OPENSSL_INCLUDE_DIR})
  set(sems_module_libs ${sems_module_libs} ${OPENSSL_LIBRARIES})
endif(OPENSSL_FOUND)

include(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
