INCLUDE(${CMAKE_SOURCE_DIR}/cmake/audio.rules.txt)

FILE (GLOB sems_core_SRCS "Am*.cpp" LowcFE.cpp SipCtrlInterface.cpp jsonArg.cpp jsonxx.cpp log.cpp md5.cpp)
FILE (GLOB sems_sip_SRCS "sip/*.cpp")

set (audio_files
beep.wav
default_en.wav
)

INCLUDE_DIRECTORIES (ampi)
INCLUDE_DIRECTORIES (amci)

ADD_LIBRARY (sems_core STATIC ${sems_core_SRCS})
ADD_LIBRARY (sems_sip STATIC ${sems_sip_SRCS})

ADD_EXECUTABLE (sems sems.cpp)

TARGET_LINK_LIBRARIES(sems ${CMAKE_DL_LIBS} sems_core sems_sip event event_pthreads -rdynamic)

IF(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
	TARGET_LINK_LIBRARIES(sems execinfo)
ELSE(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
	TARGET_LINK_LIBRARIES(sems resolv)
ENDIF(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")

IF(SPANDSP_FOUND)
	TARGET_LINK_LIBRARIES(sems ${CMAKE_DL_LIBS} spandsp)
ENDIF(SPANDSP_FOUND)

IF(LIBSAMPLERATE_FOUND)
	TARGET_LINK_LIBRARIES(sems ${CMAKE_DL_LIBS} samplerate)
ENDIF(LIBSAMPLERATE_FOUND)

IF(LIBZRTP_FOUND)
	TARGET_LINK_LIBRARIES(sems ${CMAKE_DL_LIBS} zrtp)
ENDIF(LIBZRTP_FOUND)

ADD_SUBDIRECTORY(plug-in)

# Create config-file from template
CONFIGURE_FILE (${CMAKE_SOURCE_DIR}/core/etc/app_mapping.conf.sample ${CMAKE_SOURCE_DIR}/core/etc/app_mapping.conf)
CONFIGURE_FILE (${CMAKE_SOURCE_DIR}/core/etc/sems.conf.cmake ${CMAKE_SOURCE_DIR}/core/etc/sems.conf)

# Installation of main SEMS executable
INSTALL(TARGETS sems RUNTIME DESTINATION ${SEMS_EXEC_PREFIX}/sbin)

# Installation of config-files
INSTALL(FILES ./etc/sems.conf DESTINATION ${SEMS_CFG_PREFIX}/etc/sems/)
INSTALL(FILES ./etc/app_mapping.conf DESTINATION ${SEMS_CFG_PREFIX}/etc/sems/etc/)
IF(LIBZRTP_FOUND)
INSTALL(FILES ./etc/zrtp.conf DESTINATION ${SEMS_CFG_PREFIX}/etc/sems/etc/)
ENDIF(LIBZRTP_FOUND)

INCLUDE(${CMAKE_SOURCE_DIR}/cmake/doc.rules.txt)

SET(doc_dirs ../doc/figures)
SET(doc_files
	../doc/Applications.txt
	../doc/Howtostart_noproxy.txt
	../doc/Howtostart_simpleproxy.txt
	../doc/Howtostart_voicemail.txt
	../doc/CHANGELOG
	../doc/COPYING
	../doc/Tuning.txt
)

SEMS_INSTALL_DOCS("${doc_files}" "${doc_dirs}")
SEMS_INSTALL_AUDIO("" "${audio_files}")
