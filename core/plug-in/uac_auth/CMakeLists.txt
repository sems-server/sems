set (uac_auth_SRCS
md5.c
UACAuth.cpp
)

add_library(uac_auth SHARED ${uac_auth_SRCS})
set_target_properties(uac_auth PROPERTIES OUTPUT_NAME "uac_auth")
set_target_properties(uac_auth PROPERTIES PREFIX "")
target_link_libraries(uac_auth ${CMAKE_DL_LIBS} pthread m)

