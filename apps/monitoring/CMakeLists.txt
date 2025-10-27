set(monitoring_SRCS Monitoring.cpp)

install(
  PROGRAMS tools/sems_get_callproperties.py tools/sems_list_active_calls.py
           tools/sems_list_calls.py tools/sems_list_finished_calls.py
  DESTINATION ${SEMS_EXEC_PREFIX}/sbin)

set(sems_module_name monitoring)
include(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
