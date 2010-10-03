set (webconference_SRCS
CallStats.cpp
RoomInfo.cpp
WebConference.cpp
WebConferenceDialog.cpp
)

set (webconference_audio_files
0.wav
1.wav
2.wav
3.wav
4.wav
5.wav
6.wav
7.wav
8.wav
9.wav
beep.wav
entering_conference.wav
first_participant.wav
pin_prompt.wav
wrong_pin.wav
)

SET(sems_module_name webconference)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
