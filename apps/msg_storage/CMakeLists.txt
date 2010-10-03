set (msg_storage_SRCS
MsgStorage.cpp
)

#extra_install = spooldir
#spooldir: 
#	mkdir -p $(DESTDIR)/var/spool/voicebox

SET(sems_module_name msg_storage)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/module.rules.txt)
