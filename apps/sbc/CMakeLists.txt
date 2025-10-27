file(GLOB sbc_SRCS "*.cpp")

add_subdirectory(call_control)
include_directories(${LIBEVENT2_INCLUDE_DIR})

set(sems_module_name sbc)
set(sbc_config_files
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
    symmetricrtp.sbcprofile)

install(
  PROGRAMS tools/sems-sbc-get-activeprofile
           tools/sems-sbc-get-regex-map-names
           tools/sems-sbc-list-profiles
           tools/sems-sbc-load-callcontrol-modules
           tools/sems-sbc-load-profile
           tools/sems-sbc-reload-profile
           tools/sems-sbc-reload-profiles
           tools/sems-sbc-set-activeprofile
           tools/sems-sbc-set-regex-map
           tools/sems-sbc-teardown-call
  DESTINATION ${SEMS_EXEC_PREFIX}/sbin)

include(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
