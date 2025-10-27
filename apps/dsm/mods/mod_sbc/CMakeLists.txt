set(mod_sbc_SRCS ModSbc.cpp)

include_directories(../../../sbc)
include_directories(../../../sbc/call_control/dsm)

set(sems_dsm_module_name mod_sbc)
include(${CMAKE_SOURCE_DIR}/cmake/dsm.lib.rules.txt)
