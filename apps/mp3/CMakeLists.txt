set (mp3_SRCS
mp3.c
)

#ifeq ($(WITH_MPG123DECODER), yes)
#	module_ldflags += -lmpg123
#	module_cflags += -DWITH_MPG123DECODER

SET(sems_module_name mp3)
SET(sems_module_libs mp3lame)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
