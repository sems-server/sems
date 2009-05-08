set (mod_uri_SRCS
ModUri.cpp
)

add_library(mod_uri SHARED ${mod_uri_SRCS})
set_target_properties(mod_uri PROPERTIES OUTPUT_NAME "mod_uri")
set_target_properties(mod_uri PROPERTIES PREFIX "")
target_link_libraries(mod_uri ${CMAKE_DL_LIBS} pthread m)

