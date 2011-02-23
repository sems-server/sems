all: make_tools

COREPATH ?= ../core
include $(COREPATH)/../Makefile.defs

sems-tools = sems-logfile-callextract

install: $(sems-tools) install_tools

make_tools: $(sems-tools)

install_tools: $(DESTDIR)$(bin-prefix)/$(bin-dir)
	-@for r in $(sems-tools)  ; do \
		$(INSTALL-TOUCH) $(DESTDIR)$(bin-prefix)/$(bin-dir)/$$r ; \
		$(INSTALL-BIN) $$r $(DESTDIR)$(bin-prefix)/$(bin-dir) ; \
	done

sems-logfile-callextract: logfile-splitter.o
	g++ -o sems-logfile-callextract logfile-splitter.o

clean:
	rm -f logfile-splitter.o sems-logfile-callextract