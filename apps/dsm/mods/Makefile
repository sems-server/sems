DSMPATH = ../
COREPATH = $(DSMPATH)/../../core
.DEFAULT_GOAL := all

include $(COREPATH)/../Makefile.defs

exclude_dsm_modules ?= mod_mysql mod_aws mod_curl mod_xml

ifneq ($(USE_MONITORING), yes)
exclude_dsm_modules += mod_monitoring
endif

$(info exclude_dsm_modules: $(exclude_dsm_modules))
dsm_modules = $(filter-out $(subst ;, ,$(exclude_dsm_modules))\
		$(wildcard Makefile*) lib CMakeLists.txt, \
		$(wildcard mod_*) )
$(info dsm_modules: $(dsm_modules))

.PHONY: all
all: modules

.PHONY: clean
clean:
	-@rm -f lib/*.so
	-@for r in $(dsm_modules) "" ; do \
		if [ -n "$$r" ]; then \
			echo "" ; \
			echo "" ; \
			COREPATH=../$(COREPATH) $(MAKE) -C $$r clean ; \
		fi ; \
	done

.PHONY: modules
modules:
	-@for r in $(dsm_modules) "" ; do \
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
	-@for r in $(dsm_modules) "" ; do \
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
