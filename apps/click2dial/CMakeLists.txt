set (click2dial_SRCS
Click2Dial.cpp
)

add_library(click2dial SHARED ${click2dial_SRCS})
set_target_properties(click2dial PROPERTIES OUTPUT_NAME "click2dial")
set_target_properties(click2dial PROPERTIES PREFIX "")
target_link_libraries(click2dial ${CMAKE_DL_LIBS} pthread m)

