set (ann_b2b_SRCS
AnnounceB2B.cpp
)

ADD_LIBRARY(ann_b2b SHARED ${ann_b2b_SRCS})
TARGET_LINK_LIBRARIES(ann_b2b ${CMAKE_DL_LIBS})

