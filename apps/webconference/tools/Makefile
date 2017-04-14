COREPATH_TOOLS ?= ../../../core
COREPATH ?= ../../../core
include $(COREPATH_TOOLS)/../Makefile.defs

wc_scripts = $(wildcard sems-webconference-*)

all: install_tools
install: install_tools

install_tools: $(DESTDIR)$(bin_prefix)/$(bin_dir)
	-@for r in $(wc_scripts)  ; do \
		x=`echo $$r | sed s/sems-/$(APP_NAME)-/g` ; \
		echo "installing $$r -> $$x" ; \
		$(INSTALL_TOUCH) $(DESTDIR)$(bin_prefix)/$(bin_dir)/$$x ; \
		$(INSTALL_BIN) $$r $(DESTDIR)$(bin_prefix)/$(bin_dir)/$$x ; \
	done


