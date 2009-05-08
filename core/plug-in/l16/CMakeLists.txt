set (l16_SRCS
 l16.c
)

add_library(l16 SHARED ${l16_SRCS})
set_target_properties(l16 PROPERTIES OUTPUT_NAME "l16")
set_target_properties(l16 PROPERTIES PREFIX "")
target_link_libraries(l16 ${CMAKE_DL_LIBS} pthread m)

