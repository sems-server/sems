set (auth_b2b_SRCS
AuthB2B.cpp
)

ADD_DEFINITIONS(-DMOD_NAME="auth_b2b")

ADD_LIBRARY(auth_b2b SHARED ${auth_b2b_SRCS})
TARGET_LINK_LIBRARIES(auth_b2b ${CMAKE_DL_LIBS})

