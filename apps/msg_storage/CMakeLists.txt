set (msg_storage_SRCS
MsgStorage.cpp
)

#extra_install = spooldir
#spooldir: 
#	mkdir -p $(DESTDIR)/var/spool/voicebox

IF(CMAKE_COMPILER_IS_GNUCC)
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DMOD_NAME=\"msg_storage\"")
ENDIF(CMAKE_COMPILER_IS_GNUCC)

IF(CMAKE_COMPILER_IS_GNUCXX)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DMOD_NAME='\"msg_storage\"'")
ENDIF(CMAKE_COMPILER_IS_GNUCXX)

add_library(msg_storage SHARED ${msg_storage_SRCS})
set_target_properties(msg_storage PROPERTIES OUTPUT_NAME "msg_storage")
set_target_properties(msg_storage PROPERTIES PREFIX "")
target_link_libraries(msg_storage ${CMAKE_DL_LIBS} pthread m)

