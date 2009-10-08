set (stats_SRCS
Statistics.cpp
StatsUDPServer.cpp
)

set (sems_stats_SRCS
query_stats.cxx
)

ADD_EXECUTABLE (sems-stats ${sems_stats_SRCS})
TARGET_LINK_LIBRARIES(sems-stats ${CMAKE_DL_LIBS} stdc++)

ADD_LIBRARY(stats SHARED ${stats_SRCS})
TARGET_LINK_LIBRARIES(stats ${CMAKE_DL_LIBS})

INSTALL(TARGETS sems-stats
	RUNTIME DESTINATION sbin
	)
