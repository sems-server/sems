set (mp3_SRCS
mp3.c
)

#ifeq ($(WITH_MPG123DECODER), yes)
#	module_ldflags += -lmpg123
#	module_cflags += -DWITH_MPG123DECODER

ADD_LIBRARY(mp3 SHARED ${mp3_SRCS})
TARGET_LINK_LIBRARIES(mp3 ${CMAKE_DL_LIBS} mp3lame)

