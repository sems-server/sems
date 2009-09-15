set (webconference_SRCS
CallStats.cpp
RoomInfo.cpp
WebConference.cpp
WebConferenceDialog.cpp
)

ADD_LIBRARY(webconference SHARED ${webconference_SRCS})
SET_TARGET_PROPERTIES(webconference PROPERTIES OUTPUT_NAME "webconference")
TARGET_LINK_LIBRARIES(webconference ${CMAKE_DL_LIBS} pthread m)

