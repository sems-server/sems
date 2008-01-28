COREPATH =../..
plug_in_name = l16

module_ldflags = 
module_cflags  = 

ifeq ($(OS),solaris)
	# For ntohs and friends.
	module_ldflags += -lsocket -lnsl
endif

include ../Makefile.audio_module
