plug_in_name = mod_sbc

DSMPATH ?= ../..
SBCPATH ?= $(DSMPATH)/../sbc

module_ldflags =
module_cflags  = -DMOD_NAME=\"$(plug_in_name)\" -I$(DSMPATH) \
	-I$(SBCPATH) -I$(SBCPATH)/call_control/dsm

COREPATH ?=$(DSMPATH)/../../core
lib_full_name = $(DSMPATH)/mods/lib/$(lib_name)
include $(DSMPATH)/mods/Makefile.dsm_module
