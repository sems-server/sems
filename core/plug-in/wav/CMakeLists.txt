set (wav_SRCS
 g711.c
 wav.c
 wav_hdr.c
)

SET(sems_module_name wav)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)

