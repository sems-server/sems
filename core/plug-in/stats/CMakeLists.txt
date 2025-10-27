# executable

set(sems_stats_SRCS query_stats.cxx)

add_executable(sems-stats ${sems_stats_SRCS})
target_link_libraries(sems-stats ${CMAKE_DL_LIBS} stdc++)

install(TARGETS sems-stats RUNTIME DESTINATION ${SEMS_EXEC_PREFIX}/sbin)

# library
set(stats_SRCS Statistics.cpp StatsUDPServer.cpp)

set(sems_module_name stats)
set(stats_doc_files README.stats)
include(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
include(${CMAKE_SOURCE_DIR}/cmake/config.rules.txt)
include(${CMAKE_SOURCE_DIR}/cmake/doc.rules.txt)
