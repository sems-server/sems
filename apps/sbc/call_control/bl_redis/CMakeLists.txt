set (cc_bl_redis_SRCS
BLRedis.cpp
RedisConnectionPool.cpp
)

INCLUDE_DIRECTORIES(${HIREDIS_INCLUDE_DIRS})

SET(sems_sbc_call_control_name cc_bl_redis)
SET(sems_sbc_module_libs ${HIREDIS_LIBRARIES})
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/sbc.call_control.rules.txt)

