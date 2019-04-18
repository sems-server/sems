set (mod_sbc_SRCS
ModSbc.cpp
)

INCLUDE_DIRECTORIES (../../../sbc)
INCLUDE_DIRECTORIES (../../../sbc/call_control/dsm)

SET(sems_dsm_module_name mod_sbc)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/dsm.lib.rules.txt)

