set (monitoring_SRCS
Monitoring.cpp
)

ADD_DEFINITIONS(-DMOD_NAME="monitoring")

ADD_LIBRARY(monitoring SHARED ${monitoring_SRCS})
TARGET_LINK_LIBRARIES(monitoring ${CMAKE_DL_LIBS})

