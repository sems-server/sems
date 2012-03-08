plug_in_name = monitoring

module_ldflags =
module_cflags  = -DMOD_NAME=\"$(plug_in_name)\"

extra_install = install_tools

COREPATH ?=../../core
include $(COREPATH)/plug-in/Makefile.app_module

install_tools:
	-@$(MAKE) -C tools/ install
