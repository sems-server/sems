# executable

set(sems-logfile-callextract_SRCS logfile-splitter.cpp)

add_executable(sems-logfile-callextract ${sems-logfile-callextract_SRCS})

install(TARGETS sems-logfile-callextract
        RUNTIME DESTINATION ${SEMS_EXEC_PREFIX}/sbin)

install(
  PROGRAMS sems-rtp-mux-get-max-frame-age-ms sems-rtp-mux-get-mtu-threshold
           sems-rtp-mux-set-max-frame-age-ms sems-rtp-mux-set-mtu-threshold
  DESTINATION ${SEMS_EXEC_PREFIX}/sbin)

include(${CMAKE_SOURCE_DIR}/cmake/config.rules.txt)
