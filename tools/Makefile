all: make_tools

COREPATH ?= ../core
include $(COREPATH)/../Makefile.defs

sems-tools = sems-logfile-callextract

install: $(sems-tools) install_tools

make_tools: $(sems-tools)

install_tools: $(DESTDIR)$(bin_prefix)/$(bin_dir)
	-@for r in $(sems-tools)  ; do \
		x=`echo $$r | sed s/sems-/$(APP_NAME)-/g` ; \
		echo "installing $$r -> $$x" ; \
		$(INSTALL_TOUCH) $(DESTDIR)$(bin_prefix)/$(bin_dir)/$$x ; \
		$(INSTALL_BIN) $$r $(DESTDIR)$(bin_prefix)/$(bin_dir)/$$x ; \
	done

sems-logfile-callextract: logfile-splitter.o
	$(LD) -o sems-logfile-callextract logfile-splitter.o -lstdc++

clean:
	rm -f logfile-splitter.o sems-logfile-callextract
