set(sip_dis_gw_SRCS
    Dis7Pdu.cpp
    DisAudio.cpp
    DisNetwork.cpp
    SipDisGw.cpp
    SipDisGwModule.cpp
    SipDisGwConfig.cpp)

set(sip_dis_gw_docs ${CMAKE_CURRENT_SOURCE_DIR}/Readme.sip_dis_gw.txt)
set(sip_dis_gw_config_files sip_dis_gw)

set(sems_module_name sip_dis_gw)
include(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
