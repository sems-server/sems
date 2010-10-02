# executable

set (sems_stats_SRCS
query_stats.cxx
)

ADD_EXECUTABLE (sems-stats ${sems_stats_SRCS})
TARGET_LINK_LIBRARIES(sems-stats ${CMAKE_DL_LIBS} stdc++)

INSTALL(TARGETS sems-stats
	RUNTIME DESTINATION ${SEMS_EXEC_PREFIX}/sbin
	)

# library
set (stats_SRCS
Statistics.cpp
StatsUDPServer.cpp
)

SET(sems_module_name stats)
SET(stats_doc_files README.stats)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/config.rules.txt)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/doc.rules.txt)
