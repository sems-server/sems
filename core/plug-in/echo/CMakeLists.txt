set (echo_SRCS
AmAudioEcho.cpp
Echo.cpp
)

ADD_LIBRARY(echo SHARED ${echo_SRCS})
SET_TARGET_PROPERTIES(echo PROPERTIES OUTPUT_NAME "echo")
TARGET_LINK_LIBRARIES(echo ${CMAKE_DL_LIBS} pthread m)

