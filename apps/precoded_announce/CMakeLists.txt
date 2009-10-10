set (precoded_announce_SRCS
PrecodedAnnounce.cpp
)

set (audio_files
test.predef
)

SET(sems_module_name precoded_announce)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/audio.rules.txt)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/config.rules.txt)
