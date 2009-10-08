set (mod_conference_SRCS
ModConference.cpp
)

ADD_LIBRARY(mod_conference SHARED ${mod_conference_SRCS})
TARGET_LINK_LIBRARIES(mod_conference ${CMAKE_DL_LIBS})

