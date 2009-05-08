set (voicebox_SRCS
Voicebox.cpp
VoiceboxDialog.cpp
)

add_library(voicebox SHARED ${voicebox_SRCS})
set_target_properties(voicebox PROPERTIES OUTPUT_NAME "voicebox")
set_target_properties(voicebox PROPERTIES PREFIX "")
target_link_libraries(voicebox ${CMAKE_DL_LIBS} pthread m)

