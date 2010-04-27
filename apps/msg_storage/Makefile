plug_in_name = msg_storage

module_ldflags =
module_cflags  = -DMOD_NAME=\"$(plug_in_name)\"

extra_install = spooldir

COREPATH ?=../../core
include $(COREPATH)/plug-in/Makefile.app_module

spooldir:
	-mkdir -p $(DESTDIR)/var/spool/voicebox
