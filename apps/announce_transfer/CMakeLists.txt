set (announce_transfer_SRCS
AnnounceTransfer.cpp
)

ADD_LIBRARY(announce_transfer SHARED ${announce_transfer_SRCS})
TARGET_LINK_LIBRARIES(announce_transfer ${CMAKE_DL_LIBS})

