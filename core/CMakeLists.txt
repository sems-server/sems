SET (sems_SRCS
 AmAdvancedAudio.cpp
 AmApi.cpp
 AmArg.cpp
 AmAudio.cpp
 AmAudioFile.cpp
 AmAudioMixIn.cpp
 AmB2ABSession.cpp
 AmB2BSession.cpp
 AmBufferedAudio.cpp
 AmCachedAudioFile.cpp
 AmCallWatcher.cpp
 AmConferenceChannel.cpp
 AmConferenceStatus.cpp
 AmConfig.cpp
 AmConfigReader.cpp
 AmDtmfDetector.cpp
 AmEvent.cpp
 AmEventDispatcher.cpp
 AmEventQueue.cpp
 AmIcmpWatcher.cpp
 AmJitterBuffer.cpp
 AmMediaProcessor.cpp
 AmMultiPartyMixer.cpp
 AmPlaylist.cpp
 AmPlayoutBuffer.cpp
 AmPlugIn.cpp
 AmPrecodedFile.cpp
 AmPromptCollection.cpp
 AmRingTone.cpp
 AmRtpAudio.cpp
 AmRtpPacket.cpp
 AmRtpReceiver.cpp
 AmRtpStream.cpp
 AmSdp.cpp
 AmServer.cpp
 AmSession.cpp
 AmSessionContainer.cpp
 AmSipDialog.cpp
 AmSipDispatcher.cpp
 AmSipMsg.cpp
 AmThread.cpp
 AmUAC.cpp
 AmUtils.cpp
 AmZRTP.cpp
 LowcFE.cpp
 log.cpp
 sems.cpp
)

INCLUDE_DIRECTORIES (ampi)
INCLUDE_DIRECTORIES (amci)

ADD_EXECUTABLE (sems ${sems_SRCS})
TARGET_LINK_LIBRARIES(sems ${CMAKE_DL_LIBS} stdc++)

IF(NOT MAX_RTP_SESSIONS)
	SET(MAX_RTP_SESSIONS 2048)
ENDIF(NOT MAX_RTP_SESSIONS)

ADD_DEFINITIONS(-DMAX_RTP_SESSIONS=${MAX_RTP_SESSIONS})

IF(SPANDSP_FOUND)
	ADD_DEFINITIONS(-DUSE_SPANDSP -D__STDC_LIMIT_MACROS)
	TARGET_LINK_LIBRARIES(sems ${CMAKE_DL_LIBS} spandsp)
ENDIF(SPANDSP_FOUND)

IF(LIBSAMPLERATE_FOUND)
	ADD_DEFINITIONS(-DUSE_LIBSAMPLERATE)
	TARGET_LINK_LIBRARIES(sems ${CMAKE_DL_LIBS} samplerate)
ENDIF(LIBSAMPLERATE_FOUND)

IF(SEMS_USE_IPV6)
	ADD_DEFINITIONS(-DSUPPORT_IPV6)
ENDIF(SEMS_USE_IPV6)

ADD_SUBDIRECTORY(plug-in)

INSTALL(TARGETS sems
	RUNTIME DESTINATION ${SEMS_EXEC_PREFIX}/sbin
	)
