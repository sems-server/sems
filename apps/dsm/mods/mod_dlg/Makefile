plug_in_name = mod_dlg

DSMPATH ?= ../..

module_ldflags =
module_cflags  = -DMOD_NAME=\"$(plug_in_name)\" -I$(DSMPATH)

COREPATH ?=$(DSMPATH)/../../core
lib_full_name = $(DSMPATH)/mods/lib/$(lib_name)
include $(COREPATH)/plug-in/Makefile.app_module

.PHONY: install
install: all $(extra_install)
	mkdir -p $(lib-prefix)/$(lib-dir)/dsm
	if [ -f "$(lib_name)" ]; then \
	 $(INSTALL-TOUCH) $(lib-prefix)/$(lib-dir)/dsm/; \
	 $(INSTALL-MODULES) $(lib_name) $(lib-prefix)/$(lib-dir)/dsm/; \
	fi
