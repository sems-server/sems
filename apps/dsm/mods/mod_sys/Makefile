plug_in_name = mod_sys

DSMPATH ?= ../..

module_ldflags =
module_cflags  = -DMOD_NAME=\"$(plug_in_name)\" -I$(DSMPATH)

COREPATH ?=$(DSMPATH)/../../core
include $(COREPATH)/plug-in/Makefile.app_module
