set (session_timer_SRCS
SessionTimer.cpp
UserTimer.cpp
)

ADD_LIBRARY(session_timer SHARED ${session_timer_SRCS})
SET_TARGET_PROPERTIES(session_timer PROPERTIES OUTPUT_NAME "session_timer")
SET_TARGET_PROPERTIES(session_timer PROPERTIES PREFIX "")
TARGET_LINK_LIBRARIES(session_timer ${CMAKE_DL_LIBS} pthread m)

