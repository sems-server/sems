set(mp3_SRCS mp3.c)

set(sems_module_name mp3)
if(MPG123_FOUND)
  set(sems_module_libs ${LAME_LIBRARIES} ${MPG123_LIBRARIES})
else(MPG123_FOUND)
  set(sems_module_libs ${LAME_LIBRARIES})
endif(MPG123_FOUND)
include(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
