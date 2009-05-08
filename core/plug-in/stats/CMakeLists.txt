set (stats_SRCS
Statistics.cpp
StatsUDPServer.cpp
)

set (sems_stats_SRCS
query_stats.cxx
)

ADD_EXECUTABLE (sems-stats ${sems_stats_SRCS})
TARGET_LINK_LIBRARIES(sems-stats ${CMAKE_DL_LIBS} stdc++ m)

add_library(stats SHARED ${stats_SRCS})
set_target_properties(stats PROPERTIES OUTPUT_NAME "stats")
set_target_properties(stats PROPERTIES PREFIX "")
target_link_libraries(stats ${CMAKE_DL_LIBS} pthread m)

