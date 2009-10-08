set (echo_SRCS
AmAudioEcho.cpp
Echo.cpp
)

ADD_LIBRARY(echo SHARED ${echo_SRCS})
TARGET_LINK_LIBRARIES(echo ${CMAKE_DL_LIBS})

