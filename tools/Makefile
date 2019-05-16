all: make_tools

COREPATH ?= ../core
include $(COREPATH)/../Makefile.defs

sems_tools = sems-logfile-callextract \
	sems-rtp-mux-get-max-frame-age-ms \
	sems-rtp-mux-get-mtu-threshold \
	sems-rtp-mux-set-max-frame-age-ms \
	sems-rtp-mux-set-mtu-threshold

install: $(sems_tools) install_tools

make_tools: $(sems_tools)

install_tools: $(DESTDIR)$(bin_prefix)/$(bin_dir)
	@set -e ; \
	for r in $(sems_tools) ; do \
		x=`echo $$r | sed s/sems-/$(APP_NAME)-/g` ; \
		echo "installing $$r -> $$x" ; \
		$(INSTALL_TOUCH) $(DESTDIR)$(bin_prefix)/$(bin_dir)/$$x ; \
		$(INSTALL_BIN) $$r $(DESTDIR)$(bin_prefix)/$(bin_dir)/$$x ; \
	done

sems-logfile-callextract: logfile-splitter.o
	$(LD) -o sems-logfile-callextract logfile-splitter.o -lstdc++

clean:
	rm -f logfile-splitter.o sems-logfile-callextract
