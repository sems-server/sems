# executable

set(sems-logfile-callextract_SRCS logfile-splitter.cpp)

add_executable(sems-logfile-callextract ${sems-logfile-callextract_SRCS})

install(TARGETS sems-logfile-callextract
        RUNTIME DESTINATION ${SEMS_EXEC_PREFIX}/sbin)

install(
  PROGRAMS sems-rtp-mux-get-max-frame-age-ms sems-rtp-mux-get-mtu-threshold
           sems-rtp-mux-set-max-frame-age-ms sems-rtp-mux-set-mtu-threshold
  DESTINATION ${SEMS_EXEC_PREFIX}/sbin)

# sems-stats: Rust port of core/plug-in/stats/query_stats.cxx. Builds as
# part of the root Cargo workspace; the install is only wired when the
# Rust toolchain is available, which is also the signal for
# core/plug-in/stats/CMakeLists.txt to skip its C++ install.
if(SEMS_RUST_OK)
  add_custom_target(sems-stats-rust ALL
    COMMAND ${CARGO} build ${CARGO_RELEASE_FLAG}
            --manifest-path ${SEMS_CARGO_WORKSPACE}/Cargo.toml
            -p sems-stats
    COMMENT "Building sems-stats (Rust)")

  install(PROGRAMS ${SEMS_CARGO_TARGET_DIR}/${CARGO_PROFILE}/sems-stats
          DESTINATION ${SEMS_EXEC_PREFIX}/sbin)

  add_test(
    NAME sems-stats-cargo-tests
    COMMAND ${CARGO} test ${CARGO_RELEASE_FLAG}
            --manifest-path ${SEMS_CARGO_WORKSPACE}/Cargo.toml
            -p sems-stats)
endif()

include(${CMAKE_SOURCE_DIR}/cmake/config.rules.txt)
