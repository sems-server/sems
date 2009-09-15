set (precoded_announce_SRCS
PrecodedAnnounce.cpp
)

ADD_DEFINITIONS(-DMOD_NAME="precoded_announce")

ADD_LIBRARY(precoded_announce SHARED ${precoded_announce_SRCS})
SET_TARGET_PROPERTIES(precoded_announce PROPERTIES OUTPUT_NAME "precoded_announce")
TARGET_LINK_LIBRARIES(precoded_announce ${CMAKE_DL_LIBS} pthread m)

