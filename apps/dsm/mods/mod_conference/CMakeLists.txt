set(mod_conference_SRCS ModConference.cpp)

include_directories(${LIBEVENT2_INCLUDE_DIR})

set(sems_dsm_module_name mod_conference)
include(${CMAKE_SOURCE_DIR}/cmake/dsm.lib.rules.txt)
