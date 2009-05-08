set (mod_conference_SRCS
ModConference.cpp
)

add_library(mod_conference SHARED ${mod_conference_SRCS})
set_target_properties(mod_conference PROPERTIES OUTPUT_NAME "mod_conference")
set_target_properties(mod_conference PROPERTIES PREFIX "")
target_link_libraries(mod_conference ${CMAKE_DL_LIBS} pthread m)

