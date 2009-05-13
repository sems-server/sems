set (mod_conference_SRCS
ModConference.cpp
)

ADD_LIBRARY(mod_conference SHARED ${mod_conference_SRCS})
SET_TARGET_PROPERTIES(mod_conference PROPERTIES OUTPUT_NAME "mod_conference")
SET_TARGET_PROPERTIES(mod_conference PROPERTIES PREFIX "")
TARGET_LINK_LIBRARIES(mod_conference ${CMAKE_DL_LIBS} pthread m)

