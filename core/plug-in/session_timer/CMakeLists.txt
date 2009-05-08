set (session_timer_SRCS
SessionTimer.cpp
UserTimer.cpp
)

add_library(session_timer SHARED ${session_timer_SRCS})
set_target_properties(session_timer PROPERTIES OUTPUT_NAME "session_timer")
set_target_properties(session_timer PROPERTIES PREFIX "")
target_link_libraries(session_timer ${CMAKE_DL_LIBS} pthread m)

