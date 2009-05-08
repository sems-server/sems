set (call_timer_SRCS
CallTimer.cpp
)

IF(CMAKE_COMPILER_IS_GNUCC)
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DMOD_NAME=\"call_timer\"")
ENDIF(CMAKE_COMPILER_IS_GNUCC)

IF(CMAKE_COMPILER_IS_GNUCXX)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DMOD_NAME='\"call_timer\"'")
ENDIF(CMAKE_COMPILER_IS_GNUCXX)

add_library(call_timer SHARED ${call_timer_SRCS})
set_target_properties(call_timer PROPERTIES OUTPUT_NAME "call_timer")
set_target_properties(call_timer PROPERTIES PREFIX "")
target_link_libraries(call_timer ${CMAKE_DL_LIBS} pthread m)

