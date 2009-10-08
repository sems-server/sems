set (voicebox_SRCS
Voicebox.cpp
VoiceboxDialog.cpp
)

ADD_LIBRARY(voicebox SHARED ${voicebox_SRCS})
TARGET_LINK_LIBRARIES(voicebox ${CMAKE_DL_LIBS})

