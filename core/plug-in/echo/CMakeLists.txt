set (echo_SRCS
AmAudioEcho.cpp
Echo.cpp
)

add_library(echo SHARED ${echo_SRCS})
set_target_properties(echo PROPERTIES OUTPUT_NAME "echo")
set_target_properties(echo PROPERTIES PREFIX "")
target_link_libraries(echo ${CMAKE_DL_LIBS} pthread m)

