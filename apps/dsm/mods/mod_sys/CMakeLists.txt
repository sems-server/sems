set (mod_sys_SRCS
ModSys.cpp
)

add_library(mod_sys SHARED ${mod_sys_SRCS})
set_target_properties(mod_sys PROPERTIES OUTPUT_NAME "mod_sys")
set_target_properties(mod_sys PROPERTIES PREFIX "")
target_link_libraries(mod_sys ${CMAKE_DL_LIBS} pthread m)

