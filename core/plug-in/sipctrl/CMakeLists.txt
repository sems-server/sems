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

add_library(sipctrl SHARED ${sipctrl_SRCS})
set_target_properties(sipctrl PROPERTIES PREFIX "")
target_link_libraries(sipctrl ${CMAKE_DL_LIBS} pthread m)
