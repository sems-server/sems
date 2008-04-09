plug_in_name = precoded_announce

module_ldflags =
module_cflags  = -DMOD_NAME=\"$(plug_in_name)\"

extra_install = $(plug_in_name)_audio

COREPATH ?=../../core
include $(COREPATH)/plug-in/Makefile.app_module
