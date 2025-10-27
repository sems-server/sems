set(rtmp_SRCS
    Rtmp.cpp
    RtmpAudio.cpp
    RtmpConnection.cpp
    RtmpSender.cpp
    RtmpServer.cpp
    RtmpSession.cpp
    RtmpUtils.cpp)

add_definitions(-DNO_CRYPTO)
set(sems_module_libs ${sems_module_libs} ${RTMP_LIBRARIES} ${SPEEX_LIBRARIES})

set(sems_module_name rtmp)
include(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
