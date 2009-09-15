set (callback_SRCS
CallBack.cpp
)

ADD_DEFINITIONS(-DMOD_NAME="callback")

ADD_LIBRARY(callback SHARED ${callback_SRCS})
SET_TARGET_PROPERTIES(callback PROPERTIES OUTPUT_NAME "callback")
TARGET_LINK_LIBRARIES(callback ${CMAKE_DL_LIBS} pthread m)

