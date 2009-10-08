set (wav_SRCS
 g711.c
 wav.c
 wav_hdr.c
)

ADD_LIBRARY(wav SHARED ${wav_SRCS})
TARGET_LINK_LIBRARIES(wav ${CMAKE_DL_LIBS})

