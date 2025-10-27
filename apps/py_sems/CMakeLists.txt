set(py_sems_SRCS PySems.cpp PySemsAudio.cpp PySemsB2ABDialog.cpp
                 PySemsB2BDialog.cpp PySemsDialog.cpp PySemsUtils.cpp)

if(FLITE_FOUND)
  include_directories(${FLITE_INCLUDE_DIR}/flite)
  add_definitions(-DPY_SEMS_WITH_TTS)
  set(sems_module_libs flite_cmu_us_kal flite_usenglish flite_cmulex
                       ${FLITE_LIBRARIES})
endif(FLITE_FOUND)

include_directories(${Python3_INCLUDE_DIRS})
link_libraries(${Python3_LIBRARIES})

add_subdirectory(sip)
include_directories(sip)

set(sems_module_name py_sems)
set(sems_module_libs sip)
include(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
include(${CMAKE_SOURCE_DIR}/cmake/python.rules.txt)

python_sources_compile(py/py_sems_log.py plug-in)
