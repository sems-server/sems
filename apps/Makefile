COREPATH ?= ../core
.DEFAULT_GOAL := all

include $(COREPATH)/../Makefile.defs

exclude_modules ?= mp3 examples py_sems xmlrpc2di
modules ?= $(filter-out $(subst ;, ,$(exclude_modules))\
		$(wildcard Makefile*) CVS, \
		$(wildcard *) examples/tutorial/cc_acc )
#modules = wav gsm ilbc bot echo announcement recorder voicemail dtmf_demo ivr

ifeq ($(OS),solaris)
# These don't build on Solaris because of missing Python, invalid make args, or other.
	exclude_modules += py_sems py_comp pin_collect mailbox ivr diameter_client conf_auth mp3 cc_acc_xmlrpc xmlrpc2di
endif

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
install-cfg: $(DESTDIR)$(cfg-target)
	-@for r in $(modules) "" ; do \
		if [ -n "$$r" ]; then \
			echo "" ; \
			echo "" ; \
			COREPATH=../$(COREPATH) $(MAKE) -C $$r install-cfg; \
		fi ; \
	done
