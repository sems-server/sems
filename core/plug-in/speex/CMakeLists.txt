set (speex_SRCS
 speex.c
)

# module_cflags  = -ansi # -DNOFPU
ADD_LIBRARY(speex SHARED ${speex_SRCS})
SET_TARGET_PROPERTIES(speex PROPERTIES OUTPUT_NAME "speex")
TARGET_LINK_LIBRARIES(speex ${CMAKE_DL_LIBS} pthread ${SPEEX_LIBRARIES} m)

