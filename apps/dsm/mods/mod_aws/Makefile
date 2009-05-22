plug_in_name = mod_aws

DSMPATH ?= ../..

module_ldflags = -laws
module_cflags  = -DMOD_NAME=\"$(plug_in_name)\" -I$(DSMPATH) 

COREPATH ?=$(DSMPATH)/../../core
lib_full_name = $(DSMPATH)/mods/lib/$(lib_name)
include $(DSMPATH)/mods/Makefile.dsm_module
