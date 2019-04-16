set (monitoring_SRCS
Monitoring.cpp
)

INSTALL(PROGRAMS
tools/sems-get-callproperties
tools/sems-list-active-calls
tools/sems-list-calls
tools/sems-list-finished-calls
	DESTINATION ${SEMS_EXEC_PREFIX}/sbin
	)

SET(sems_module_name monitoring)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
