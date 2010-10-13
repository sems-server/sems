SET(sems_module_name conf_auth)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/config.rules.txt)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/python.rules.txt)

PYTHON_SOURCES_COMPILE(conf_auth.py ivr)
SEMS_INSTALL_CONFIGS(conf_auth)
