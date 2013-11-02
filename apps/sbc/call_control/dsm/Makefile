plug_in_name = cc_dsm

SEMSPATH ?= ../../../..
COREPATH ?= $(SEMSPATH)/core
sbc_app_path = $(SEMSPATH)/apps/sbc
dsm_app_path = $(SEMSPATH)/apps/dsm

module_ldflags =
module_cflags  = -DMOD_NAME=\"$(plug_in_name)\"  -I$(sbc_app_path) -I$(dsm_app_path)

include $(COREPATH)/plug-in/Makefile.app_module
