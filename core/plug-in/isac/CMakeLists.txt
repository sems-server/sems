FILE (GLOB isac_SRCS isac.c "libisac/*.c")
INCLUDE_DIRECTORIES (libisac)

SET(sems_module_name isac)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
