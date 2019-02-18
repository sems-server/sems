plug_in_name = g729
COREPATH =../..

module_ldflags = -lbcg729
module_cflags  =

ifdef NOFPU
	module_cflags += -DNOFPU
endif

include ../Makefile.audio_module
