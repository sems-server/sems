set (uac_auth_SRCS
md5.c
UACAuth.cpp
)

ADD_LIBRARY(uac_auth SHARED ${uac_auth_SRCS})
SET_TARGET_PROPERTIES(uac_auth PROPERTIES OUTPUT_NAME "uac_auth")
TARGET_LINK_LIBRARIES(uac_auth ${CMAKE_DL_LIBS} pthread m)

