set (cc_dsm_SRCS
CCDSM.cpp
SBCDSMInstance.cpp
)

INCLUDE_DIRECTORIES (../../../dsm)

SET(sems_sbc_call_control_name cc_dsm)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/sbc.call_control.rules.txt)
