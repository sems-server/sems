set(sems_module_name conf_auth)
include(${CMAKE_SOURCE_DIR}/cmake/config.rules.txt)
include(${CMAKE_SOURCE_DIR}/cmake/python.rules.txt)

python_sources_compile(conf_auth.py ivr)
sems_install_configs(conf_auth)
