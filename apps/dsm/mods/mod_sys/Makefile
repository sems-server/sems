plug_in_name = mod_sys

DSMPATH ?= ../..

module_ldflags =
module_cflags  = -DMOD_NAME=\"$(plug_in_name)\" -I$(DSMPATH)

COREPATH ?=$(DSMPATH)/../../core
lib_full_name = $(DSMPATH)/mods/lib/$(lib_name)
include $(DSMPATH)/mods/Makefile.dsm_module
