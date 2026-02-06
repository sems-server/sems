include(${CMAKE_SOURCE_DIR}/cmake/audio.rules.txt)
include(${CMAKE_SOURCE_DIR}/cmake/template.rules.txt)

file(
  GLOB
  sems_core_SRCS
  "Am*.cpp"
  LowcFE.cpp
  SipCtrlInterface.cpp
  crc4.cpp
  jsonArg.cpp
  jsonxx.cpp
  log.cpp
  md5.cpp)
file(GLOB sems_sip_SRCS "sip/*.cpp")
file(GLOB sems_tests_SRCS "tests/*.cpp" "plug-in/uac_auth/UACAuth.cpp"
     "../apps/sbc/*.cpp")

set(audio_files beep.wav default_en.wav)

include_directories(ampi)
include_directories(amci)

add_library(sems_core STATIC ${sems_core_SRCS})
add_library(sems_sip STATIC ${sems_sip_SRCS})

add_executable(sems sems.cpp)
add_executable(sems_tests ${sems_tests_SRCS})

foreach(EXE_TARGET sems sems_tests)

  # This allows symbols defined in the SIP stack but not used by the core itself
  # to be included in the executable and thus be available for modules
  if(APPLE)
    target_link_libraries(
      ${EXE_TARGET}
      -Wl,-force_load
      sems_core
      -Wl,-force_load
      sems_sip
      ${CMAKE_DL_LIBS}
      event
      event_pthreads
      -rdynamic)
  else()
    target_link_libraries(
      ${EXE_TARGET}
      -Wl,--whole-archive
      sems_core
      sems_sip
      -Wl,--no-whole-archive
      ${CMAKE_DL_LIBS}
      event
      event_pthreads
      -rdynamic)
  endif()

  if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    target_link_libraries(${EXE_TARGET} execinfo)
  else(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    target_link_libraries(${EXE_TARGET} resolv)
  endif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")

  if(SPANDSP_FOUND)
    target_link_libraries(${EXE_TARGET} ${CMAKE_DL_LIBS} spandsp)
  endif(SPANDSP_FOUND)

  if(LIBSAMPLERATE_FOUND)
    target_link_libraries(${EXE_TARGET} ${CMAKE_DL_LIBS}
                          ${LIBSAMPLERATE_LIBRARIES})
  else(LIBSAMPLERATE_FOUND)
    target_sources(${EXE_TARGET} PRIVATE "resample/resample.cpp")
  endif(LIBSAMPLERATE_FOUND)

  if(LIBZRTP_FOUND)
    target_link_libraries(${EXE_TARGET} ${CMAKE_DL_LIBS} ${LIBZRTP_LIBRARIES})
  endif(LIBZRTP_FOUND)

endforeach(EXE_TARGET)

add_subdirectory(plug-in)

# Enable testing
add_test(NAME Tests COMMAND sems_tests)

# Create config-file from template
configure_file(${CMAKE_SOURCE_DIR}/core/etc/app_mapping.conf.sample
               ${CMAKE_SOURCE_DIR}/core/etc/app_mapping.conf)
configure_file(${CMAKE_SOURCE_DIR}/core/etc/sems.conf.sample
               ${CMAKE_SOURCE_DIR}/core/etc/sems.conf)

# We use custom regen instead of *.conf templates to use it for both traditional
# make and CMake
sems_gen_template(${CMAKE_SOURCE_DIR}/core/etc/sems.conf)
sems_gen_template(${CMAKE_SOURCE_DIR}/core/sems.h)

# Installation of main SEMS executable
install(TARGETS sems RUNTIME DESTINATION ${SEMS_EXEC_PREFIX}/sbin)

# Installation of config-files
install(FILES ./etc/sems.conf DESTINATION ${SEMS_CFG_PREFIX}/etc/sems/)
install(FILES ./etc/app_mapping.conf
        DESTINATION ${SEMS_CFG_PREFIX}/etc/sems/etc/)
if(LIBZRTP_FOUND)
  install(FILES ./etc/zrtp.conf DESTINATION ${SEMS_CFG_PREFIX}/etc/sems/etc/)
endif(LIBZRTP_FOUND)

include(${CMAKE_SOURCE_DIR}/cmake/doc.rules.txt)

set(doc_dirs ../doc/figures)
set(doc_files
    ../doc/Applications.txt
    ../doc/Howtostart_noproxy.txt
    ../doc/Howtostart_simpleproxy.txt
    ../doc/Howtostart_voicemail.txt
    ../doc/CHANGELOG
    ../doc/COPYING
    ../doc/Tuning.txt)

sems_install_docs("${doc_files}" "${doc_dirs}")
sems_install_audio("" "${audio_files}")
