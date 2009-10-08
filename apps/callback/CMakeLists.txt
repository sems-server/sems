set (callback_SRCS
CallBack.cpp
)

ADD_DEFINITIONS(-DMOD_NAME="callback")

ADD_LIBRARY(callback SHARED ${callback_SRCS})
TARGET_LINK_LIBRARIES(callback ${CMAKE_DL_LIBS})

