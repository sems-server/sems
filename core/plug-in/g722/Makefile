COREPATH =../..

plug_in_name = g722

SPANDSP_INC=
#/usr/include/
SPANDSP_LIB = -lspandsp

module_ldflags = $(SPANDSP_LIB)
module_cflags  = -ansi # -DNOFPU

ifdef NOFPU
	module_cflags += -DNOFPU
endif

include ../Makefile.audio_module
