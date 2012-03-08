COREPATH_TOOLS ?= ../../../core
include $(COREPATH_TOOLS)/../Makefile.defs

monitoring_scripts = $(wildcard sems-*)

all: install_tools
install: install_tools

install_tools: $(DESTDIR)$(bin-prefix)/$(bin-dir)
	-@for r in $(monitoring_scripts)  ; do \
		$(INSTALL-TOUCH) $(DESTDIR)$(bin-prefix)/$(bin-dir)/$$r ; \
		$(INSTALL-BIN) $$r $(DESTDIR)$(bin-prefix)/$(bin-dir) ; \
	done


