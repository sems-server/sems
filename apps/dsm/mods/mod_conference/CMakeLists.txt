set (mod_conference_SRCS
ModConference.cpp
)

INCLUDE_DIRECTORIES(${LIBEVENT2_INCLUDE_DIR})

SET(sems_dsm_module_name mod_conference)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/dsm.lib.rules.txt)


