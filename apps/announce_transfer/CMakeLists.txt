set (announce_transfer_SRCS
AnnounceTransfer.cpp
)

add_library(announce_transfer SHARED ${announce_transfer_SRCS})
set_target_properties(announce_transfer PROPERTIES OUTPUT_NAME "announce_transfer")
set_target_properties(announce_transfer PROPERTIES PREFIX "")
target_link_libraries(announce_transfer ${CMAKE_DL_LIBS} pthread m)

