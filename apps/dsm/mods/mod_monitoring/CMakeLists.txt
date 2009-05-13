set (mod_monitoring_SRCS
ModMonitoring.cpp
)

ADD_LIBRARY(mod_monitoring SHARED ${mod_monitoring_SRCS})
SET_TARGET_PROPERTIES(mod_monitoring PROPERTIES OUTPUT_NAME "mod_monitoring")
SET_TARGET_PROPERTIES(mod_monitoring PROPERTIES PREFIX "")
TARGET_LINK_LIBRARIES(mod_monitoring ${CMAKE_DL_LIBS} pthread m)

