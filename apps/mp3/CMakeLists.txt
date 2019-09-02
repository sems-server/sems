set (mp3_SRCS
mp3.c
)

SET(sems_module_name mp3)
IF(MPG123_FOUND)
	SET(sems_module_libs ${LAME_LIBRARIES} ${MPG123_LIBRARIES})
ELSE(MPG123_FOUND)
	SET(sems_module_libs ${LAME_LIBRARIES})
ENDIF(MPG123_FOUND)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
