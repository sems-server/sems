file(GLOB isac_SRCS isac.c "libisac/*.c")
include_directories(libisac)

set(sems_module_name isac)
include(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
