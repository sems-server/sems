set (announcement_SRCS
Announcement.cpp
)

ADD_LIBRARY(announcement SHARED ${announcement_SRCS})
SET_TARGET_PROPERTIES(announcement PROPERTIES OUTPUT_NAME "announcement")
SET_TARGET_PROPERTIES(announcement PROPERTIES PREFIX "")
TARGET_LINK_LIBRARIES(announcement ${CMAKE_DL_LIBS} pthread m)

