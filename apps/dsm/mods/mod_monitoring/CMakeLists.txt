set (mod_monitoring_SRCS
ModMonitoring.cpp
)

add_library(mod_monitoring SHARED ${mod_monitoring_SRCS})
set_target_properties(mod_monitoring PROPERTIES OUTPUT_NAME "mod_monitoring")
set_target_properties(mod_monitoring PROPERTIES PREFIX "")
target_link_libraries(mod_monitoring ${CMAKE_DL_LIBS} pthread m)

