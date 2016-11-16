set (mp3_SRCS
mp3.c
)

#ifeq ($(WITH_MPG123DECODER), yes)
#	module_ldflags += -lmpg123
#	module_cflags += -DWITH_MPG123DECODER

SET(sems_module_name mp3)
IF(MPG123_FOUND)
	SET(sems_module_libs mp3lame mpg123)
ELSE(MPG123_FOUND)
	SET(sems_module_libs mp3lame)
ENDIF(MPG123_FOUND)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
