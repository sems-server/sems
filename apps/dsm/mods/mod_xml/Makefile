plug_in_name = mod_xml

DSMPATH ?= ../..

module_ldflags = `xml2-config --libs`
module_cflags  = -DMOD_NAME=\"$(plug_in_name)\" -I$(DSMPATH) \
	`xml2-config --cflags`

COREPATH ?=$(DSMPATH)/../../core
lib_full_name = $(DSMPATH)/mods/lib/$(lib_name)
include $(DSMPATH)/mods/Makefile.dsm_module
