set (sbc_SRCS
HeaderFilter.cpp
ParamReplacer.cpp
RTPParameters.cpp
RegexMapper.cpp
SBC.cpp
SBCCallProfile.cpp
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
symmetricrtp.sbcprofile
)

INSTALL(PROGRAMS
tools/sems-sbc-get-activeprofile
tools/sems-sbc-get-regex-map-names
tools/sems-sbc-list-profiles
tools/sems-sbc-load-profile
tools/sems-sbc-reload-profile
tools/sems-sbc-reload-profiles
tools/sems-sbc-set-activeprofile
tools/sems-sbc-set-regex-map
	DESTINATION ${SEMS_EXEC_PREFIX}/sbin
	)

SET(sems_module_name sbc)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
