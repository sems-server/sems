set (annrecorder_SRCS
AnnRecorder.cpp
)

add_library(annrecorder SHARED ${annrecorder_SRCS})
set_target_properties(annrecorder PROPERTIES OUTPUT_NAME "annrecorder")
set_target_properties(annrecorder PROPERTIES PREFIX "")
target_link_libraries(annrecorder ${CMAKE_DL_LIBS} pthread m)

