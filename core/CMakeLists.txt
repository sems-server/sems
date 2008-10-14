
set (sems_SRCS
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

include_directories (${SEMS_SOURCE_DIR}/core/ampi)
include_directories (${SEMS_SOURCE_DIR}/core/amci)

configure_file( config.h.cmake ${SEMS_SOURCE_DIR}/core/config.h )

add_executable (sems ${sems_SRCS})
target_link_libraries(sems ${CMAKE_DL_LIBS} pthread stdc++ m)

add_subdirectory (plug-in)
