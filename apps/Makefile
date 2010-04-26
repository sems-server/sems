COREPATH ?= ../core
.DEFAULT_GOAL := all

include $(COREPATH)/../Makefile.defs

exclude_app_modules += mp3 examples py_sems xmlrpc2di gateway jsonrpc

ifneq ($(USE_MONITORING), yes)
exclude_app_modules += monitoring
endif

$(info exclude_app_modules: $(exclude_app_modules))
app_modules = $(filter-out $(subst ;, ,$(exclude_app_modules))\
		$(wildcard Makefile*) CVS CMakeLists.txt, \
		$(wildcard *) examples/tutorial/cc_acc )

#modules = wav gsm ilbc bot echo announcement recorder voicemail dtmf_demo ivr

.PHONY: all
all: modules

.PHONY: clean
clean:
	-@rm -f *.so
	-@for r in $(app_modules) "" ; do \
		if [ -n "$$r" ]; then \
			echo "" ; \
			echo "" ; \
			COREPATH=../$(COREPATH) $(MAKE) -C $$r clean ; \
		fi ; \
	done

.PHONY: modules
modules:
	-@for r in $(app_modules) "" ; do \
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
	-@for r in $(app_modules) "" ; do \
		if [ -n "$$r" ]; then \
			echo "" ; \
			echo "" ; \
			COREPATH=../$(COREPATH) $(MAKE) -C $$r install; \
		fi ; \
	done

.PHONY: install-cfg
install-cfg: $(DESTDIR)$(cfg-target)
	-@for r in $(app_modules) "" ; do \
		if [ -n "$$r" ]; then \
			echo "" ; \
			echo "" ; \
			COREPATH=../$(COREPATH) $(MAKE) -C $$r install-cfg; \
		fi ; \
	done
