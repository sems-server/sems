set(mod_redis_SRCS DRedisConnection.cpp ModRedis.cpp)

include_directories(${HIREDIS_INCLUDE_DIRS})

set(sems_dsm_module_name mod_redis)
set(sems_dsm_module_libs ${HIREDIS_LIBRARIES})
include(${CMAKE_SOURCE_DIR}/cmake/dsm.lib.rules.txt)
