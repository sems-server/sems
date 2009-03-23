plug_in_name = mod_mysql

DSMPATH ?= ../..

module_ldflags = -lmysqlpp
module_cflags  = -DMOD_NAME=\"$(plug_in_name)\" -I$(DSMPATH) -I/usr/include/mysql++ -I/usr/include/mysql

COREPATH ?=$(DSMPATH)/../../core
lib_full_name = $(DSMPATH)/mods/lib/$(lib_name)
include $(DSMPATH)/mods/Makefile.dsm_module
