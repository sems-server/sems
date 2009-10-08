set (speex_SRCS
 speex.c
)

INCLUDE_DIRECTORIES(${SPEEX_INCLUDE_DIR})

# module_cflags  = -ansi # -DNOFPU
ADD_LIBRARY(libspeex SHARED ${speex_SRCS})
SET_TARGET_PROPERTIES(libspeex PROPERTIES OUTPUT_NAME "speex")
TARGET_LINK_LIBRARIES(libspeex ${CMAKE_DL_LIBS} speex)

