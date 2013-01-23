COREPATH_TOOLS ?= ../../../core
include $(COREPATH_TOOLS)/../Makefile.defs

sbc_scripts = $(wildcard sems-sbc-*)

all: install_tools
install: install_tools

install_tools: $(DESTDIR)$(bin-prefix)/$(bin-dir)
	-@for r in $(sbc_scripts)  ; do \
		x=`echo $$r | sed s/sems-/$(APP_NAME)-/g` ; \
		echo "installing $$r -> $$x" ; \
		$(INSTALL-TOUCH) $(DESTDIR)$(bin-prefix)/$(bin-dir)/$$x ; \
		$(INSTALL-BIN) $$r $(DESTDIR)$(bin-prefix)/$(bin-dir)/$$x ; \
	done


