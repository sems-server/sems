set(early_announce_SRCS EarlyAnnounce.cpp)

if(MYSQLCPPCONN_FOUND)
  include_directories(${MYSQLCPPCONN_INCLUDE_DIR})
  add_definitions(-DWITH_MYSQL)
  set(sems_module_libs ${MYSQLCPPCONN_LIBRARIES})
endif(MYSQLCPPCONN_FOUND)

set(sems_module_name early_announce)
include(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
