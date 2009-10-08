set (g722_SRCS
g722.c
)

#module_cflags  = -ansi # -DNOFPU
ADD_LIBRARY(g722 SHARED ${g722_SRCS})
TARGET_LINK_LIBRARIES(g722 ${CMAKE_DL_LIBS} spandsp)

