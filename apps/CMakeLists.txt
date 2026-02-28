set(EXCLUDE_APP_MODULES "" CACHE STRING
    "Space- or semicolon-separated list of application modules to exclude from the build")

# Normalize: replace spaces with semicolons to form a proper CMake list
string(REPLACE " " ";" _exclude_app_list "${EXCLUDE_APP_MODULES}")

macro(sems_add_app_module mod)
  if(NOT "${mod}" IN_LIST _exclude_app_list)
    add_subdirectory(${mod})
  else()
    message(STATUS "Excluding app module: ${mod}")
  endif()
endmacro()

set(EXTRA_APP_MODULES "" CACHE STRING
    "Space- or semicolon-separated list of extra apps to include to the build")

# Normalize: replace spaces with semicolons to form a proper CMake list
string(REPLACE " " ";" _extra_app_list "${EXTRA_APP_MODULES}")

foreach(mod IN LISTS _extra_app_list)
    message(STATUS "Including extra app module: ${mod}")
    sems_add_app_module(${mod})
endforeach()

sems_add_app_module(ann_b2b)
sems_add_app_module(announce_transfer)
sems_add_app_module(announcement)
sems_add_app_module(annrecorder)
sems_add_app_module(callback)
sems_add_app_module(click2dial)
if(LIBEVENT2_FOUND)
  sems_add_app_module(conference)
endif(LIBEVENT2_FOUND)
if(MYSQLPP_FOUND)
  sems_add_app_module(db_reg_agent)
endif(MYSQLPP_FOUND)
sems_add_app_module(diameter_client)
sems_add_app_module(dsm)
sems_add_app_module(early_announce)
sems_add_app_module(early_dbprompt)
# ADD_SUBDIRECTORY(examples)
if(MISDN_FOUND)
  # ADD_SUBDIRECTORY(gateway)
endif(MISDN_FOUND)
if(LIBEV_FOUND)
  sems_add_app_module(jsonrpc)
endif(LIBEV_FOUND)
sems_add_app_module(mobile_push)
sems_add_app_module(monitoring)
if(LAME_FOUND)
  sems_add_app_module(mp3)
endif(LAME_FOUND)
sems_add_app_module(msg_storage)
sems_add_app_module(mwi)
sems_add_app_module(precoded_announce)
sems_add_app_module(reg_agent)
sems_add_app_module(registrar_client)
if(RTMP_FOUND AND SPEEX_FOUND)
  sems_add_app_module(rtmp)
endif(RTMP_FOUND AND SPEEX_FOUND)
sems_add_app_module(sbc)
sems_add_app_module(siprec_srs)
sems_add_app_module(voicebox)
sems_add_app_module(voicemail)
sems_add_app_module(webconference)
sems_add_app_module(xmlrpc2di)

# Python-dependent modules
if(Python3_FOUND)
  sems_add_app_module(conf_auth)
  sems_add_app_module(ivr)
  sems_add_app_module(mailbox)
  sems_add_app_module(pin_collect)

  if(SIP_FOUND)
    message(
      WARNING
        "The py_sems module will not be built because the code has not been upgraded to interface with Python3 SIP."
    )
    # ADD_SUBDIRECTORY(py_sems)
  endif(SIP_FOUND)

  # ADD_SUBDIRECTORY(twit)
endif(Python3_FOUND)
