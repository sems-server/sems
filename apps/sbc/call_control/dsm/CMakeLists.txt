set(cc_dsm_SRCS CCDSM.cpp SBCDSMInstance.cpp)

include_directories(../../../dsm)

set(sems_sbc_call_control_name cc_dsm)
include(${CMAKE_SOURCE_DIR}/cmake/sbc.call_control.rules.txt)
