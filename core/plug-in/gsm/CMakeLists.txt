set(gsm_SRCS gsm.c)

include_directories(${GSM_INCLUDE_DIR})

set(sems_module_name gsm)
set(sems_module_libs ${GSM_LIBRARIES})
include(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
