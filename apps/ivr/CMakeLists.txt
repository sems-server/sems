set (ivr_SRCS
Ivr.cpp
IvrAudio.cpp
IvrAudioMixIn.cpp
IvrDialogBase.cpp
IvrSipDialog.cpp
IvrSipReply.cpp
IvrSipRequest.cpp
IvrUAC.cpp
)

IF(FLITE_FOUND)
	INCLUDE_DIRECTORIES(${FLITE_INCLUDE_DIR}/flite)
	ADD_DEFINITIONS(-DIVR_WITH_TTS)
	SET(sems_module_libs flite_cmu_us_kal flite_usenglish flite_cmulex flite)
ENDIF(FLITE_FOUND)

INCLUDE_DIRECTORIES(${PYTHON_INCLUDE_PATH})
EXECUTE_PROCESS(COMMAND python-config --libs OUTPUT_VARIABLE PYTHON_LIB_FLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${PYTHON_LIB_FLAGS}")

SET(sems_module_name ivr)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/python.rules.txt)

PYTHON_SOURCES_COMPILE(py/log.py ivr)
