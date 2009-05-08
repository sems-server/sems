set (announcement_SRCS
Announcement.cpp
)

add_library(announcement SHARED ${announcement_SRCS})
set_target_properties(announcement PROPERTIES OUTPUT_NAME "announcement")
set_target_properties(announcement PROPERTIES PREFIX "")
target_link_libraries(announcement ${CMAKE_DL_LIBS} pthread m)

