set (voicebox_SRCS
Voicebox.cpp
VoiceboxDialog.cpp
)

ADD_LIBRARY(voicebox SHARED ${voicebox_SRCS})
SET_TARGET_PROPERTIES(voicebox PROPERTIES OUTPUT_NAME "voicebox")
TARGET_LINK_LIBRARIES(voicebox ${CMAKE_DL_LIBS} pthread m)

