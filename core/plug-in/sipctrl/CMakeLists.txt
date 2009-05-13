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

ADD_LIBRARY(sipctrl SHARED ${sipctrl_SRCS})
SET_TARGET_PROPERTIES(sipctrl PROPERTIES OUTPUT_NAME "sipctrl")
SET_TARGET_PROPERTIES(sipctrl PROPERTIES PREFIX "")
TARGET_LINK_LIBRARIES(sipctrl ${CMAKE_DL_LIBS} pthread m)
