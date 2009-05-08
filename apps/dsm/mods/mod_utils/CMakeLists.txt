set (mod_utils_SRCS
ModUtils.cpp
)

add_library(mod_utils SHARED ${mod_utils_SRCS})
set_target_properties(mod_utils PROPERTIES OUTPUT_NAME "mod_utils")
set_target_properties(mod_utils PROPERTIES PREFIX "")
target_link_libraries(mod_utils ${CMAKE_DL_LIBS} pthread m)

