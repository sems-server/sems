set (annrecorder_SRCS
AnnRecorder.cpp
)

ADD_LIBRARY(annrecorder SHARED ${annrecorder_SRCS})
TARGET_LINK_LIBRARIES(annrecorder ${CMAKE_DL_LIBS})

