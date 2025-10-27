set(mod_py_SRCS ModPy.cpp PyDSM.cpp PyDSMSession.cpp)

include_directories(${Python3_INCLUDE_DIRS})
link_libraries(${Python3_LIBRARIES})

set(sems_dsm_module_name mod_py)
include(${CMAKE_SOURCE_DIR}/cmake/dsm.lib.rules.txt)
