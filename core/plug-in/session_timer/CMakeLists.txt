set (session_timer_SRCS
SessionTimer.cpp
UserTimer.cpp
)

ADD_LIBRARY(session_timer SHARED ${session_timer_SRCS})
TARGET_LINK_LIBRARIES(session_timer ${CMAKE_DL_LIBS})

