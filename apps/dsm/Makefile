plug_in_name = dsm

module_ldflags =
module_cflags  = 
#-DMOD_NAME=\"$(plug_in_name)\" 

extra_target = make_dsm_mods
extra_install = $(plug_in_name)_lib install_dsm_mods
extra_clean = clean_dsm_mods

COREPATH ?=../../core
include $(COREPATH)/plug-in/Makefile.app_module

make_dsm_mods: 
	-@$(MAKE) -C mods  

install_dsm_mods:
	-@$(MAKE) -C mods/ install

clean_dsm_mods:
	-@$(MAKE) -C mods/ clean
