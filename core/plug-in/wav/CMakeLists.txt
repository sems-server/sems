set (wav_SRCS
 g711.c
 wav.c
 wav_hdr.c
)

add_library(wav SHARED ${wav_SRCS})
set_target_properties(wav PROPERTIES OUTPUT_NAME "wav")
set_target_properties(wav PROPERTIES PREFIX "")
target_link_libraries(wav ${CMAKE_DL_LIBS} pthread stdc++ m)

