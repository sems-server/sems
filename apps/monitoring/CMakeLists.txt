set(monitoring_SRCS Monitoring.cpp)

install(
  PROGRAMS tools/sems_get_callproperties.py tools/sems_list_active_calls.py
           tools/sems_list_calls.py tools/sems_list_finished_calls.py
  DESTINATION ${SEMS_EXEC_PREFIX}/sbin)

# Rust-based monitoring tools (require rustc >= 1.38 for edition 2018).
# Rust detection + shared cargo settings live in the root CMakeLists.txt so
# tools/ and apps/monitoring/tools/ can share one workspace and one target/.
if(SEMS_RUST_OK)
  set(SEMS_MONITORING_BINARIES
    sems-list-calls
    sems-list-active-calls
    sems-list-finished-calls
    sems-get-callproperties
    sems-prometheus-exporter)

  set(SEMS_MONITORING_CARGO_ARGS "")
  foreach(bin ${SEMS_MONITORING_BINARIES})
    list(APPEND SEMS_MONITORING_CARGO_ARGS "-p" "${bin}")
  endforeach()

  add_custom_target(sems-monitoring-tools ALL
    COMMAND ${CARGO} build ${CARGO_RELEASE_FLAG}
            --manifest-path ${SEMS_CARGO_WORKSPACE}/Cargo.toml
            ${SEMS_MONITORING_CARGO_ARGS}
    COMMENT "Building sems monitoring tools (Rust)")

  foreach(bin ${SEMS_MONITORING_BINARIES})
    install(PROGRAMS ${SEMS_CARGO_TARGET_DIR}/${CARGO_PROFILE}/${bin}
            DESTINATION ${SEMS_EXEC_PREFIX}/sbin)
  endforeach()
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
