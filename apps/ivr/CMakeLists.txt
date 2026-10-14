file(GLOB ivr_SRCS "*.cpp")

if(FLITE_FOUND)
  include_directories(${FLITE_INCLUDE_DIR}/flite)
  add_definitions(-DIVR_WITH_TTS)
  set(sems_module_libs flite_cmu_us_kal flite_usenglish flite_cmulex
                       ${FLITE_LIBRARIES})
endif(FLITE_FOUND)

include_directories(${Python3_INCLUDE_DIRS})
link_libraries(${Python3_LIBRARIES})

set(sems_module_name ivr)
include(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
include(${CMAKE_SOURCE_DIR}/cmake/python.rules.txt)

python_sources_compile(py/log.py ivr)

# PythonScriptThread is tested on its own, against an embedded interpreter
# and AmThread, without the rest of the core; the test does its own logging
add_executable(ivr_tests tests/ivr_tests.cpp PythonScriptThread.cpp
                         ${CMAKE_SOURCE_DIR}/core/AmThread.cpp)
target_include_directories(ivr_tests PRIVATE ${CMAKE_SOURCE_DIR}/core/tests)
target_link_libraries(ivr_tests ${Python3_LIBRARIES} ${CMAKE_DL_LIBS} pthread)
add_test(NAME IvrTests COMMAND ivr_tests)
