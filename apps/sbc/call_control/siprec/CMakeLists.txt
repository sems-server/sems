set(cc_siprec_SRCS
  CCSiprec.cpp
  SiprecDialog.cpp
  SiprecMetadata.cpp
  RtpForwarder.cpp
)

set(sems_sbc_call_control_name cc_siprec)
include(${CMAKE_SOURCE_DIR}/cmake/sbc.call_control.rules.txt)
