# executable

set(sems_stats_SRCS query_stats.cxx)

add_executable(sems-stats ${sems_stats_SRCS})
target_link_libraries(sems-stats ${CMAKE_DL_LIBS} stdc++)

# When the Rust port (tools/sems-stats) is built it installs its own binary
# to the same path, so skip installing this C++ version to avoid the
# collision. The C++ target is still built so it remains a working fallback
# and a reference implementation. SEMS_RUST_OK is set in the root CMakeLists.
if(NOT SEMS_RUST_OK)
  install(TARGETS sems-stats RUNTIME DESTINATION ${SEMS_EXEC_PREFIX}/sbin)
endif()

# library
set(stats_SRCS Statistics.cpp StatsUDPServer.cpp)

set(sems_module_name stats)
set(stats_doc_files README.stats)
include(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
include(${CMAKE_SOURCE_DIR}/cmake/config.rules.txt)
include(${CMAKE_SOURCE_DIR}/cmake/doc.rules.txt)
