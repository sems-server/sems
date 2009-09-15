set (stats_SRCS
Statistics.cpp
StatsUDPServer.cpp
)

set (sems_stats_SRCS
query_stats.cxx
)

ADD_EXECUTABLE (sems-stats ${sems_stats_SRCS})
TARGET_LINK_LIBRARIES(sems-stats ${CMAKE_DL_LIBS} stdc++ m)

ADD_LIBRARY(stats SHARED ${stats_SRCS})
SET_TARGET_PROPERTIES(stats PROPERTIES OUTPUT_NAME "stats")
TARGET_LINK_LIBRARIES(stats ${CMAKE_DL_LIBS} pthread m)

INSTALL(TARGETS sems-stats
	RUNTIME DESTINATION sbin
	)
