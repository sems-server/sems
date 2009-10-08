set (sw_prepaid_sip_SRCS
SWPrepaidSIP.cpp
)

ADD_LIBRARY(sw_prepaid_sip SHARED ${sw_prepaid_sip_SRCS})
TARGET_LINK_LIBRARIES(sw_prepaid_sip ${CMAKE_DL_LIBS})

