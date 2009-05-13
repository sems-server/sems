set (announce_transfer_SRCS
AnnounceTransfer.cpp
)

ADD_LIBRARY(announce_transfer SHARED ${announce_transfer_SRCS})
SET_TARGET_PROPERTIES(announce_transfer PROPERTIES OUTPUT_NAME "announce_transfer")
SET_TARGET_PROPERTIES(announce_transfer PROPERTIES PREFIX "")
TARGET_LINK_LIBRARIES(announce_transfer ${CMAKE_DL_LIBS} pthread m)

