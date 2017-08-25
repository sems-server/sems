COREPATH_TOOLS ?= ../../../core
COREPATH?=$(COREPATH_TOOLS)
include $(COREPATH_TOOLS)/../Makefile.defs


sbc_scripts = $(wildcard sems-sbc-*)

all: install_tools
install: install_tools

install_tools: $(DESTDIR)$(bin_prefix)/$(bin_dir)
	@set -e ; \
	for r in $(sbc_scripts)  ; do \
		x=`echo $$r | sed s/sems-/$(APP_NAME)-/g` ; \
		echo "installing $$r -> $$x" ; \
		$(INSTALL_TOUCH) $(DESTDIR)$(bin_prefix)/$(bin_dir)/$$x ; \
		$(INSTALL_BIN) $$r $(DESTDIR)$(bin_prefix)/$(bin_dir)/$$x ; \
	done


