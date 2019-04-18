set (mod_redis_SRCS
DRedisConnection.cpp
ModRedis.cpp
)

INCLUDE_DIRECTORIES(${HIREDIS_INCLUDE_DIRS})

SET(sems_dsm_module_name mod_redis)
SET(sems_dsm_module_libs ${HIREDIS_LIBRARIES})
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/dsm.lib.rules.txt)

