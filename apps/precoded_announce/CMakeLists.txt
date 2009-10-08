set (precoded_announce_SRCS
PrecodedAnnounce.cpp
)

ADD_DEFINITIONS(-DMOD_NAME="precoded_announce")

ADD_LIBRARY(precoded_announce SHARED ${precoded_announce_SRCS})
TARGET_LINK_LIBRARIES(precoded_announce ${CMAKE_DL_LIBS})

