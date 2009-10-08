set (click2dial_SRCS
Click2Dial.cpp
)

ADD_LIBRARY(click2dial SHARED ${click2dial_SRCS})
TARGET_LINK_LIBRARIES(click2dial ${CMAKE_DL_LIBS})

