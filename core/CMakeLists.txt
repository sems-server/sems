
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
TARGET_LINK_LIBRARIES(sems ${CMAKE_DL_LIBS} pthread stdc++ m)

IF(SPANDSP_FOUND)
	IF(CMAKE_COMPILER_IS_GNUCC)
		SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_SPANDSP -D__STDC_LIMIT_MACROS")
	ENDIF(CMAKE_COMPILER_IS_GNUCC)

	IF(CMAKE_COMPILER_IS_GNUCXX)
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_SPANDSP -D__STDC_LIMIT_MACROS")
	ENDIF(CMAKE_COMPILER_IS_GNUCXX)
	TARGET_LINK_LIBRARIES(sems ${CMAKE_DL_LIBS} spandsp)
ENDIF(SPANDSP_FOUND)

IF(LIBSAMPLERATE_FOUND)
	IF(CMAKE_COMPILER_IS_GNUCC)
		SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_LIBSAMPLERATE")
	ENDIF(CMAKE_COMPILER_IS_GNUCC)

	IF(CMAKE_COMPILER_IS_GNUCXX)
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_LIBSAMPLERATE")
	ENDIF(CMAKE_COMPILER_IS_GNUCXX)
	TARGET_LINK_LIBRARIES(sems ${CMAKE_DL_LIBS} samplerate)
ENDIF(LIBSAMPLERATE_FOUND)

ADD_SUBDIRECTORY(plug-in)

