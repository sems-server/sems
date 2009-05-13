set (click2dial_SRCS
Click2Dial.cpp
)

ADD_LIBRARY(click2dial SHARED ${click2dial_SRCS})
SET_TARGET_PROPERTIES(click2dial PROPERTIES OUTPUT_NAME "click2dial")
SET_TARGET_PROPERTIES(click2dial PROPERTIES PREFIX "")
TARGET_LINK_LIBRARIES(click2dial ${CMAKE_DL_LIBS} pthread m)

