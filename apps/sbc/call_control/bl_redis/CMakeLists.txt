set(cc_bl_redis_SRCS BLRedis.cpp RedisConnectionPool.cpp)

include_directories(${HIREDIS_INCLUDE_DIRS})

set(sems_sbc_call_control_name cc_bl_redis)
set(sems_sbc_module_libs ${HIREDIS_LIBRARIES})
include(${CMAKE_SOURCE_DIR}/cmake/sbc.call_control.rules.txt)
