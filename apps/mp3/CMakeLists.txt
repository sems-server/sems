set (mp3_SRCS
mp3.c
)

#ifeq ($(WITH_MPG123DECODER), yes)
#	module_ldflags += -lmpg123
#	module_cflags += -DWITH_MPG123DECODER

add_library(mp3 SHARED ${mp3_SRCS})
set_target_properties(mp3 PROPERTIES OUTPUT_NAME "mp3")
set_target_properties(mp3 PROPERTIES PREFIX "")
target_link_libraries(mp3 ${CMAKE_DL_LIBS} pthread m mp3lame)

