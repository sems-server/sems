plug_in_name = sbc

module_ldflags =
module_cflags  =

extra_target = make_call_control_mods
extra_install = install_tools install_call_control_mods
extra_clean = clean_call_control_mods

COREPATH ?= ../../core
include $(COREPATH)/plug-in/Makefile.app_module

install_tools:
	-@$(MAKE) -C tools/ install

make_call_control_mods:
	-@$(MAKE) -C call_control

install_call_control_mods:
	-@$(MAKE) -C call_control/ install

clean_call_control_mods:
	-@$(MAKE) -C call_control/ clean


