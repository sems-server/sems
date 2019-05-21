set (gsm_SRCS
 gsm.c
)

INCLUDE_DIRECTORIES(${GSM_INCLUDE_DIR})

SET(sems_module_name gsm)
SET(sems_module_libs ${GSM_LIBRARIES})
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
