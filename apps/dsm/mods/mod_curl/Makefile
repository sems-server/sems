plug_in_name = mod_curl

DSMPATH ?= ../..

module_ldflags = `curl-config --libs`
module_cflags  = -DMOD_NAME=\"$(plug_in_name)\" -I$(DSMPATH) `curl-config --cflags`

# for local libcurl:
#module_cflags  = -DMOD_NAME=\"$(plug_in_name)\" -I$(DSMPATH) -Icurl-7.15.5/include
#module_ldflags = curl-7.15.5/lib/.libs/libcurl.a

COREPATH ?=$(DSMPATH)/../../core
lib_full_name = $(DSMPATH)/mods/lib/$(lib_name)
include $(DSMPATH)/mods/Makefile.dsm_module
