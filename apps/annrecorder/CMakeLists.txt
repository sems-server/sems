set (annrecorder_SRCS
AnnRecorder.cpp
)

ADD_LIBRARY(annrecorder SHARED ${annrecorder_SRCS})
SET_TARGET_PROPERTIES(annrecorder PROPERTIES OUTPUT_NAME "annrecorder")
TARGET_LINK_LIBRARIES(annrecorder ${CMAKE_DL_LIBS} pthread m)

