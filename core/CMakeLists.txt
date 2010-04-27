SET (sems_SRCS
 AmAdvancedAudio.cpp
 AmApi.cpp
 AmArg.cpp
 AmAudio.cpp
 AmAudioFile.cpp
 AmAudioMixIn.cpp
 AmAudioMixer.cpp
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
 AmSession.cpp
 AmSessionContainer.cpp
 AmSessionEventHandler.cpp
 AmSessionProcessor.cpp
 AmSipDialog.cpp
 AmSipDispatcher.cpp
 AmSipMsg.cpp
 AmThread.cpp
 AmUAC.cpp
 AmUriParser.cpp
 AmUtils.cpp
 AmZRTP.cpp
 LowcFE.cpp
 log.cpp
 sems.cpp
 SipCtrlInterface.cpp
 sip/hash.cpp
 sip/hash_table.cpp
 sip/msg_fline.cpp
 sip/msg_hdrs.cpp
 sip/parse_common.cpp
 sip/parse_cseq.cpp
 sip/parse_from_to.cpp
 sip/parse_header.cpp
 sip/parse_uri.cpp
 sip/parse_via.cpp
 sip/resolver.cpp
 sip/sip_parser.cpp
 sip/sip_trans.cpp
 sip/trans_layer.cpp
 sip/transport.cpp
 sip/udp_trsp.cpp
 sip/wheeltimer.cpp
)

set (audio_files
beep.wav
default_en.wav
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

# Create config-file from template
CONFIGURE_FILE (${CMAKE_SOURCE_DIR}/core/etc/sems.conf.cmake ${CMAKE_SOURCE_DIR}/core/etc/sems.conf)

# Installation of main SEMS executable
INSTALL(TARGETS sems RUNTIME DESTINATION ${SEMS_EXEC_PREFIX}/sbin)

# Installation of config-files
INSTALL(FILES ./etc/sems.conf DESTINATION ${SEMS_CFG_PREFIX}/etc/sems/)
INSTALL(FILES ./etc/app_mapping.conf DESTINATION ${SEMS_CFG_PREFIX}/etc/sems/etc/)
IF(ZRTP_FOUND)
INSTALL(FILES ./etc/zrtp.conf DESTINATION ${SEMS_CFG_PREFIX}/etc/sems/etc/)
ENDIF(ZRTP_FOUND)

SET(doc_dirs ../doc/figures)
SET(doc_files
	../doc/CHANGELOG
	../doc/COMPILING
	../doc/COPYING
	../doc/Readme.ann_b2b.txt
	../doc/Readme.announce_transfer.txt
	../doc/Readme.announcement.txt
	../doc/Readme.annrecorder.txt
	../doc/Readme.auth_b2b.txt
	../doc/Readme.call_timer.txt
	../doc/Readme.callback.txt
	../doc/Readme.click2dial.txt
	../doc/Readme.conf_auth.txt
	../doc/Readme.conference.txt
	../doc/Readme.diameter_client.txt
	../doc/Readme.early_announce.txt
	../doc/Readme.echo.txt
	../doc/Readme.iLBC.txt
	../doc/Readme.ivr.txt
	../doc/Readme.mailbox.txt
	../doc/Readme.monitoring.txt
	../doc/Readme.mp3plugin.txt
	../doc/Readme.msg_storage.txt
	../doc/Readme.pin_collect.txt
	../doc/Readme.py_sems.txt
	../doc/Readme.reg_agent.txt
	../doc/Readme.registrar_client.txt
	../doc/Readme.sw_prepaid_sip.txt
	../doc/Readme.twit.txt
	../doc/Readme.uac_auth.txt
	../doc/Readme.voicebox.txt
	../doc/Readme.voicemail.txt
	../doc/Readme.webconference.txt
	../doc/Readme.xmlrpc2di.txt
	../doc/WHATSNEW_0.10
)

INCLUDE(${CMAKE_SOURCE_DIR}/cmake/audio.rules.txt)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/doc.rules.txt)
