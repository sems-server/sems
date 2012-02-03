COREPATH = ../../../core
.DEFAULT_GOAL := all

include $(COREPATH)/../Makefile.defs

exclude_cc_modules ?= template bl_redis

$(info exclude_cc_modules: $(exclude_cc_modules))
cc_modules = $(filter-out $(subst ;, ,$(exclude_cc_modules))\
		$(wildcard Makefile*) CMakeLists.txt, \
		$(wildcard *) )
$(info cc_modules: $(cc_modules))

.PHONY: all
all: modules

.PHONY: clean
clean:
	-@for r in $(cc_modules) "" ; do \
		if [ -n "$$r" ]; then \
			echo "" ; \
			echo "" ; \
			COREPATH=../$(COREPATH) $(MAKE) -C $$r clean ; \
		fi ; \
	done

.PHONY: modules
modules:
	-@for r in $(cc_modules) "" ; do \
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
	-@for r in $(cc_modules) "" ; do \
		if [ -n "$$r" ]; then \
			echo "" ; \
			echo "" ; \
			COREPATH=../$(COREPATH) $(MAKE) -C $$r install; \
		fi ; \
	done

.PHONY: install-cfg
install-cfg: $(DESTDIR)$(cfg-target)
	-@for r in $(dsm_modules) "" ; do \
		if [ -n "$$r" ]; then \
			echo "" ; \
			echo "" ; \
			COREPATH=../$(COREPATH) $(MAKE) -C $$r install-cfg; \
		fi ; \
	done
