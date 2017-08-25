plug_in_name = webconference

module_ldflags =
module_cflags  = 

extra_install = $(plug_in_name)_audio install_tools

COREPATH ?=../../core
include $(COREPATH)/plug-in/Makefile.app_module

install_tools:
	@$(MAKE) -C tools/ install
