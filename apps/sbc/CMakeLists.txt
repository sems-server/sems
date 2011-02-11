set (sbc_SRCS
SBC.cpp
SBCCallProfile.cpp
HeaderFilter.cpp
ParamReplacer.cpp
SDPFilter.cpp
)

SET(sbc_config_files
auth_b2b.sbcprofile
call_timer.sbcprofile
codecfilter.sbcprofile
prepaid.sbcprofile
refuse.sbcprofile
replytranslate.sbcprofile
sbc
src_ipmap
sst_b2b.sbcprofile
transparent.sbcprofile
)
SET(sems_module_name sbc)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
