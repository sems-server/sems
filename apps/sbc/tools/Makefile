COREPATH ?= ../../../core
include $(COREPATH)/../Makefile.defs

sbc_scripts = $(wildcard sems-sbc-*)

all: install_tools
install: install_tools

install_tools: $(DESTDIR)$(bin-prefix)/$(bin-dir)
	-@for r in $(sbc_scripts)  ; do \
		$(INSTALL-TOUCH) $(DESTDIR)$(bin-prefix)/$(bin-dir)/$$r ; \
		$(INSTALL-BIN) $$r $(DESTDIR)$(bin-prefix)/$(bin-dir) ; \
	done


