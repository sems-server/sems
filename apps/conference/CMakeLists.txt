set(conference_SRCS Conference.cpp)

set(conference_audio_files beep.wav first_participant.wav)

include_directories(${LIBEVENT2_INCLUDE_DIR})

if(FLITE_FOUND AND ZRTP_FOUND)
  include_directories(${FLITE_INCLUDE_DIR}/flite)
  add_definitions(-DWITH_SAS_TTS)
  set(sems_module_libs ${sems_module_libs} flite_cmu_us_kal flite_usenglish
                       flite_cmulex ${FLITE_LIBRARIES})
endif(FLITE_FOUND AND ZRTP_FOUND)

if(MYSQLPP_FOUND)
  include_directories(/usr/include/mysql)
  include_directories(${MYSQLPP_INCLUDE_DIR}/mysql++)
  add_definitions(-DUSE_MYSQL)
  set(sems_module_libs ${sems_module_libs} ${MYSQLPP_LIBRARIES})
endif(MYSQLPP_FOUND)

set(sems_module_name conference)
include(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
