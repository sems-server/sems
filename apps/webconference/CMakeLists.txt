set (webconference_SRCS
CallStats.cpp
RoomInfo.cpp
WebConference.cpp
WebConferenceDialog.cpp
)

ADD_LIBRARY(webconference SHARED ${webconference_SRCS})
TARGET_LINK_LIBRARIES(webconference ${CMAKE_DL_LIBS})

