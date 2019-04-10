FILE (GLOB sbc_SRCS "*.cpp")

ADD_SUBDIRECTORY (call_control)

SET(sems_module_name sbc)
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
tools/sems-sbc-load-callcontrol-modules
tools/sems-sbc-load-profile
tools/sems-sbc-reload-profile
tools/sems-sbc-reload-profiles
tools/sems-sbc-set-activeprofile
tools/sems-sbc-set-regex-map
tools/sems-sbc-teardown-call
	DESTINATION ${SEMS_EXEC_PREFIX}/sbin
	)

INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
