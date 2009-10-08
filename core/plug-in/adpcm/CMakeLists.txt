set (adpcm_SRCS
g711.c
g721.c
g723_16.c
g723_24.c
g723_40.c
g72x.c
)

ADD_LIBRARY(adpcm SHARED ${adpcm_SRCS})
TARGET_LINK_LIBRARIES(adpcm ${CMAKE_DL_LIBS})

