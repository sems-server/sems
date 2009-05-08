set (webconference_SRCS
CallStats.cpp
RoomInfo.cpp
WebConference.cpp
WebConferenceDialog.cpp
)

add_library(webconference SHARED ${webconference_SRCS})
set_target_properties(webconference PROPERTIES OUTPUT_NAME "webconference")
set_target_properties(webconference PROPERTIES PREFIX "")
target_link_libraries(webconference ${CMAKE_DL_LIBS} pthread m)

