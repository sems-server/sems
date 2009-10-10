set (sipctrl_SRCS
  SipCtrlInterface.cpp
  hash.cpp
  hash_table.cpp
  main.cpp
  msg_fline.cpp
  msg_hdrs.cpp
  parse_common.cpp
  parse_cseq.cpp
  parse_from_to.cpp
  parse_header.cpp
  parse_uri.cpp
  parse_via.cpp
  resolver.cpp
  sip_parser.cpp
  sip_trans.cpp
  trans_layer.cpp
  transport.cpp
  udp_trsp.cpp
  wheeltimer.cpp
)

IF(SEMS_USE_IPV6)
	ADD_DEFINITIONS(-DSUPPORT_IPV6)
ENDIF(SEMS_USE_IPV6)

SET(sems_module_name sipctrl)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/config.rules.txt)
