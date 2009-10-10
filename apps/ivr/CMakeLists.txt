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

INCLUDE_DIRECTORIES(${PYTHON_INCLUDE_PATH})

IF(FLITE_FOUND)
	INCLUDE_DIRECTORIES(${FLITE_INCLUDE_DIR}/flite)
	ADD_DEFINITIONS(-DIVR_WITH_TTS)
	SET(sems_module_libs flite_cmu_us_kal flite_usenglish flite_cmulex flite)
ENDIF(FLITE_FOUND)

SET(sems_module_name ivr)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/config.rules.txt)
