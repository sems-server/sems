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

ADD_LIBRARY(ivr SHARED ${ivr_SRCS})
TARGET_LINK_LIBRARIES(ivr ${CMAKE_DL_LIBS})

IF(FLITE_FOUND)
	INCLUDE_DIRECTORIES(${FLITE_INCLUDE_DIR}/flite)
	ADD_DEFINITIONS(-DIVR_WITH_TTS)
	TARGET_LINK_LIBRARIES(ivr ${CMAKE_DL_LIBS} flite_cmu_us_kal flite_usenglish flite_cmulex flite)
ENDIF(FLITE_FOUND)

