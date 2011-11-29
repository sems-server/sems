set (rtmp_SRCS
Rtmp.cpp
RtmpAudio.cpp
RtmpConnection.cpp
RtmpSender.cpp
RtmpServer.cpp
RtmpSession.cpp
RtmpUtils.cpp
)

ADD_DEFINITIONS(-DNO_CRYPTO)
SET(sems_module_libs ${sems_module_libs} rtmp speex)

SET(sems_module_name rtmp)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
