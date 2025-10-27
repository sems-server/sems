set(cc_syslog_cdr_SRCS SyslogCDR.cpp)

set(sems_sbc_call_control_name cc_syslog_cdr)
include(${CMAKE_SOURCE_DIR}/cmake/sbc.call_control.rules.txt)
