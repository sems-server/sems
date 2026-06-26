set(dis_test_SRCS
    DisTest.cpp)

include_directories(${OPENDIS_INCLUDE_DIRS})
set(sems_module_libs ${sems_module_libs} ${OPENDIS_LIBRARIES})

set(dis_test_docs ${CMAKE_CURRENT_SOURCE_DIR}/Readme.dis_test.txt)
set(dis_test_config_files dis_test)

set(sems_module_name dis_test)
include(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
