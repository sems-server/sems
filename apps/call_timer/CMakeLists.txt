set (call_timer_SRCS
CallTimer.cpp
)

ADD_DEFINITIONS(-DMOD_NAME="call_timer")

ADD_LIBRARY(call_timer SHARED ${call_timer_SRCS})
TARGET_LINK_LIBRARIES(call_timer ${CMAKE_DL_LIBS})

