INCLUDE(${CMAKE_SOURCE_DIR}/cmake/audio.rules.txt)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/template.rules.txt)

FILE (GLOB sems_core_SRCS "Am*.cpp" LowcFE.cpp SipCtrlInterface.cpp crc4.cpp jsonArg.cpp jsonxx.cpp log.cpp md5.cpp)
FILE (GLOB sems_sip_SRCS "sip/*.cpp")
FILE (GLOB sems_tests_SRCS "tests/*.cpp" "plug-in/uac_auth/UACAuth.cpp" "../apps/sbc/*.cpp")

set (audio_files
beep.wav
default_en.wav
)

INCLUDE_DIRECTORIES (ampi)
INCLUDE_DIRECTORIES (amci)

ADD_LIBRARY (sems_core STATIC ${sems_core_SRCS})
ADD_LIBRARY (sems_sip STATIC ${sems_sip_SRCS})

ADD_EXECUTABLE (sems sems.cpp)
ADD_EXECUTABLE(sems_tests ${sems_tests_SRCS})

FOREACH (EXE_TARGET sems sems_tests)

	# This allows symbols defined in the SIP stack but not used
	# by the core itself to be included in the executable and
	# thus be available for modules
	IF(APPLE)
		TARGET_LINK_LIBRARIES(${EXE_TARGET} ${CMAKE_DL_LIBS} -Wl,-force_load sems_core -Wl,-force_load sems_sip event event_pthreads -rdynamic)
	ELSE()
		TARGET_LINK_LIBRARIES(${EXE_TARGET} ${CMAKE_DL_LIBS} -Wl,--whole-archive sems_core sems_sip -Wl,--no-whole-archive event event_pthreads -rdynamic)
	ENDIF()

	IF(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
		TARGET_LINK_LIBRARIES(${EXE_TARGET} execinfo)
	ELSE(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
		TARGET_LINK_LIBRARIES(${EXE_TARGET} resolv)
	ENDIF(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")

	IF(SPANDSP_FOUND)
		TARGET_LINK_LIBRARIES(${EXE_TARGET} ${CMAKE_DL_LIBS} spandsp)
	ENDIF(SPANDSP_FOUND)

	IF(LIBSAMPLERATE_FOUND)
		TARGET_LINK_LIBRARIES(${EXE_TARGET} ${CMAKE_DL_LIBS} ${LIBSAMPLERATE_LIBRARIES})
	ELSE(LIBSAMPLERATE_FOUND)
		TARGET_SOURCES (${EXE_TARGET} PRIVATE "resample/resample.cpp")
	ENDIF(LIBSAMPLERATE_FOUND)

	IF(LIBZRTP_FOUND)
		TARGET_LINK_LIBRARIES(${EXE_TARGET} ${CMAKE_DL_LIBS} ${LIBZRTP_LIBRARIES})
	ENDIF(LIBZRTP_FOUND)

ENDFOREACH (EXE_TARGET)

ADD_SUBDIRECTORY(plug-in)

# Enable testing
add_test(NAME Tests COMMAND sems_tests)

# Create config-file from template
CONFIGURE_FILE (${CMAKE_SOURCE_DIR}/core/etc/app_mapping.conf.sample ${CMAKE_SOURCE_DIR}/core/etc/app_mapping.conf)
CONFIGURE_FILE (${CMAKE_SOURCE_DIR}/core/etc/sems.conf.sample ${CMAKE_SOURCE_DIR}/core/etc/sems.conf)

# We use custom regen instead of *.conf templates to use it for both traditional make and CMake
SEMS_GEN_TEMPLATE(${CMAKE_SOURCE_DIR}/core/etc/sems.conf)
SEMS_GEN_TEMPLATE(${CMAKE_SOURCE_DIR}/core/sems.h)

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
