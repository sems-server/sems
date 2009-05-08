set (adpcm_SRCS
g711.c
g721.c
g723_16.c
g723_24.c
g723_40.c
g72x.c
)

add_library(adpcm SHARED ${adpcm_SRCS})
set_target_properties(adpcm PROPERTIES OUTPUT_NAME "adpcm")
set_target_properties(adpcm PROPERTIES PREFIX "")
target_link_libraries(adpcm ${CMAKE_DL_LIBS} pthread m)

