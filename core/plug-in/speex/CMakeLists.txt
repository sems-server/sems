set (speex_SRCS
 speex.c
)

# module_cflags  = -ansi # -DNOFPU
add_library(speex SHARED ${speex_SRCS})
set_target_properties(speex PROPERTIES OUTPUT_NAME "speex")
set_target_properties(speex PROPERTIES PREFIX "")
target_link_libraries(speex ${CMAKE_DL_LIBS} pthread ${SPEEX_LIBRARIES} m)

