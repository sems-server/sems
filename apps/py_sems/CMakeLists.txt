set (py_sems_SRCS
PySems.cpp
PySemsAudio.cpp
PySemsB2ABDialog.cpp
PySemsB2BDialog.cpp
PySemsDialog.cpp
PySemsUtils.cpp
)

IF(FLITE_FOUND)
	INCLUDE_DIRECTORIES(${FLITE_INCLUDE_DIR}/flite)
	ADD_DEFINITIONS(-DPY_SEMS_WITH_TTS)
	SET(sems_module_libs flite_cmu_us_kal flite_usenglish flite_cmulex flite)
ENDIF(FLITE_FOUND)

INCLUDE_DIRECTORIES(${PYTHON_INCLUDE_PATH})
EXECUTE_PROCESS(COMMAND python-config --libs OUTPUT_VARIABLE PYTHON_LIB_FLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${PYTHON_LIB_FLAGS}")

ADD_SUBDIRECTORY (sip)
INCLUDE_DIRECTORIES (sip)

SET(sems_module_name py_sems)
SET(sems_module_libs sip)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/config.rules.txt)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/python.rules.txt)

PYTHON_SOURCES_COMPILE(py/py_sems_log.py plug-in)
