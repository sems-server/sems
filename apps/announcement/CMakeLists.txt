set (announcement_SRCS
Announcement.cpp
)

ADD_LIBRARY(announcement SHARED ${announcement_SRCS})
TARGET_LINK_LIBRARIES(announcement ${CMAKE_DL_LIBS})

