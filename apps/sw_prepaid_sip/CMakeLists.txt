set (sw_prepaid_sip_SRCS
SWPrepaidSIP.cpp
)

add_library(sw_prepaid_sip SHARED ${sw_prepaid_sip_SRCS})
set_target_properties(sw_prepaid_sip PROPERTIES OUTPUT_NAME "sw_prepaid_sip")
set_target_properties(sw_prepaid_sip PROPERTIES PREFIX "")
target_link_libraries(sw_prepaid_sip ${CMAKE_DL_LIBS} pthread m)

