plug_in_name = jsonrpc

module_ldflags = -lev
module_cflags  = -DMOD_NAME=\"$(plug_in_name)\" -fno-strict-aliasing

COREPATH ?=../../core
include $(COREPATH)/plug-in/Makefile.app_module
