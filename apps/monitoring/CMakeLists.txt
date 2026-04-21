set(monitoring_SRCS Monitoring.cpp)

install(
  PROGRAMS tools/sems_get_callproperties.py tools/sems_list_active_calls.py
           tools/sems_list_calls.py tools/sems_list_finished_calls.py
  DESTINATION ${SEMS_EXEC_PREFIX}/sbin)

# Rust-based monitoring tools (require rustc >= 1.38 for edition 2018)
find_program(CARGO cargo)
set(SEMS_RUST_OK FALSE)
if(CARGO)
  execute_process(
    COMMAND rustc --version
    OUTPUT_VARIABLE RUSTC_VERSION_OUTPUT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE RUSTC_RESULT)
  if(RUSTC_RESULT EQUAL 0)
    string(REGEX MATCH "[0-9]+\\.[0-9]+" RUSTC_VERSION "${RUSTC_VERSION_OUTPUT}")
    if(RUSTC_VERSION VERSION_GREATER_EQUAL "1.38")
      set(SEMS_RUST_OK TRUE)
      message(STATUS "Found cargo: ${CARGO}, rustc ${RUSTC_VERSION}")
    else()
      message(WARNING "rustc ${RUSTC_VERSION} found but >= 1.38 required; skipping Rust monitoring tools")
    endif()
  endif()
endif()

# Expose to other subdirectories (e.g. core/plug-in/stats, which has its own
# C++ sems-stats binary and needs to know whether the Rust port is taking
# over the installed path).
set(SEMS_HAS_RUST ${SEMS_RUST_OK} CACHE INTERNAL "Rust toolchain detected and usable")

if(SEMS_RUST_OK)
  set(SEMS_MONITORING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tools)

  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CARGO_PROFILE "debug")
    set(CARGO_RELEASE_FLAG "")
  else()
    set(CARGO_PROFILE "release")
    set(CARGO_RELEASE_FLAG "--release")
  endif()

  set(SEMS_MONITORING_BINARIES
    sems-list-calls
    sems-list-active-calls
    sems-list-finished-calls
    sems-get-callproperties
    sems-prometheus-exporter
    sems-stats)

  add_custom_target(sems-monitoring-tools ALL
    COMMAND ${CARGO} build ${CARGO_RELEASE_FLAG}
            --manifest-path ${SEMS_MONITORING_DIR}/Cargo.toml
    COMMENT "Building sems monitoring tools (Rust)")

  foreach(bin ${SEMS_MONITORING_BINARIES})
    install(PROGRAMS ${SEMS_MONITORING_DIR}/target/${CARGO_PROFILE}/${bin}
            DESTINATION ${SEMS_EXEC_PREFIX}/sbin)
  endforeach()

  # Run `cargo test` as part of `ctest`. Scoped to the sems-stats crate to
  # keep the test surface aligned with what this commit adds; extend the
  # `-p` list if other crates grow their own tests.
  add_test(
    NAME sems-stats-cargo-tests
    COMMAND ${CARGO} test ${CARGO_RELEASE_FLAG}
            --manifest-path ${SEMS_MONITORING_DIR}/Cargo.toml
            -p sems-stats)
else()
  message(WARNING "cargo/rustc not found or too old; installing Python wrapper scripts for monitoring tools")

  set(MONITORING_FALLBACKS
    "sems-list-calls:sems_list_calls.py"
    "sems-list-active-calls:sems_list_active_calls.py"
    "sems-list-finished-calls:sems_list_finished_calls.py"
    "sems-get-callproperties:sems_get_callproperties.py")

  foreach(entry ${MONITORING_FALLBACKS})
    string(REPLACE ":" ";" parts ${entry})
    list(GET parts 0 bin_name)
    list(GET parts 1 py_name)
    set(wrapper_path "${CMAKE_CURRENT_BINARY_DIR}/${bin_name}")
    file(WRITE ${wrapper_path}
      "#!/bin/sh\nexec \"$(dirname \"$0\")/${py_name}\" \"$@\"\n")
    install(PROGRAMS ${wrapper_path}
            DESTINATION ${SEMS_EXEC_PREFIX}/sbin)
  endforeach()
endif()

set(sems_module_name monitoring)
include(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
