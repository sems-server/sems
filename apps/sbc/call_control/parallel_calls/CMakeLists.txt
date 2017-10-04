set (cc_parallel_calls_SRCS
CCParallelCalls.cpp
)

SET(sems_sbc_call_control_name cc_parallel_calls)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/sbc.call_control.rules.txt)
