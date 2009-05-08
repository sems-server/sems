set (g722_SRCS
g722.c
)

#module_cflags  = -ansi # -DNOFPU
add_library(g722 SHARED ${g722_SRCS})
set_target_properties(g722 PROPERTIES OUTPUT_NAME "g722")
set_target_properties(g722 PROPERTIES PREFIX "")
target_link_libraries(g722 ${CMAKE_DL_LIBS} pthread ${SPANDSP_LIBRARIES} m)

