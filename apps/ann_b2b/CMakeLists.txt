set (ann_b2b_SRCS
AnnounceB2B.cpp
)

add_library(ann_b2b SHARED ${ann_b2b_SRCS})
set_target_properties(ann_b2b PROPERTIES OUTPUT_NAME "ann_b2b")
set_target_properties(ann_b2b PROPERTIES PREFIX "")
target_link_libraries(ann_b2b ${CMAKE_DL_LIBS} pthread m)

