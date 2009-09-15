set (monitoring_SRCS
Monitoring.cpp
)

ADD_DEFINITIONS(-DMOD_NAME="monitoring")

ADD_LIBRARY(monitoring SHARED ${monitoring_SRCS})
SET_TARGET_PROPERTIES(monitoring PROPERTIES OUTPUT_NAME "monitoring")
TARGET_LINK_LIBRARIES(monitoring ${CMAKE_DL_LIBS} pthread m)

