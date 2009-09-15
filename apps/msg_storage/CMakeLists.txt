set (msg_storage_SRCS
MsgStorage.cpp
)

#extra_install = spooldir
#spooldir: 
#	mkdir -p $(DESTDIR)/var/spool/voicebox

ADD_DEFINITIONS(-DMOD_NAME="msg_storage")

ADD_LIBRARY(msg_storage SHARED ${msg_storage_SRCS})
SET_TARGET_PROPERTIES(msg_storage PROPERTIES OUTPUT_NAME "msg_storage")
TARGET_LINK_LIBRARIES(msg_storage ${CMAKE_DL_LIBS} pthread m)

