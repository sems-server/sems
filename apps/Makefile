COREPATH ?= ../core
.DEFAULT_GOAL := all

include $(COREPATH)/../Makefile.defs

exclude_modules = mp3 examples
modules = $(filter-out $(exclude_modules) \
		$(wildcard Makefile*) CVS, \
		$(wildcard *) ) examples/tutorial/cc_acc
#modules = wav gsm ilbc bot echo announcement recorder voicemail dtmf_demo ivr

.PHONY: all
all: modules

.PHONY: clean
clean:
	-@rm -f *.so
	-@for r in $(modules) "" ; do \
		if [ -n "$$r" ]; then \
			echo "" ; \
			echo "" ; \
			COREPATH=../$(COREPATH) $(MAKE) -C $$r clean ; \
		fi ; \
	done

.PHONY: modules
modules:
	-@for r in $(modules) "" ; do \
		if [ -n "$$r" ]; then \
			echo  "" ; \
			echo  "" ; \
			COREPATH=../$(COREPATH) $(MAKE) -C $$r all; \
		fi ; \
	done 

.PHONY: install
install: install-bin install-cfg

.PHONY: install-bin
install-bin:
	-@for r in $(modules) "" ; do \
		if [ -n "$$r" ]; then \
			echo "" ; \
			echo "" ; \
			COREPATH=../$(COREPATH) $(MAKE) -C $$r install; \
		fi ; \
	done

.PHONY: install-cfg
install-cfg: $(cfg-prefix)/$(cfg-dir)
	-@for r in $(modules) "" ; do \
		if [ -n "$$r" ]; then \
			echo "" ; \
			echo "" ; \
			COREPATH=../$(COREPATH) $(MAKE) -C $$r install-cfg; \
		fi ; \
	done
