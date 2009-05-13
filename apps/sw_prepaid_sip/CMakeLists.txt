set (sw_prepaid_sip_SRCS
SWPrepaidSIP.cpp
)

ADD_LIBRARY(sw_prepaid_sip SHARED ${sw_prepaid_sip_SRCS})
SET_TARGET_PROPERTIES(sw_prepaid_sip PROPERTIES OUTPUT_NAME "sw_prepaid_sip")
SET_TARGET_PROPERTIES(sw_prepaid_sip PROPERTIES PREFIX "")
TARGET_LINK_LIBRARIES(sw_prepaid_sip ${CMAKE_DL_LIBS} pthread m)

