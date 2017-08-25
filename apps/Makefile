COREPATH ?= ../core
.DEFAULT_GOAL := all

local_exclude_app_modules =
# Do not have a Makefile:
local_exclude_app_modules += confpin
local_exclude_app_modules += early_dbprompt
local_exclude_app_modules += mobile_push
# Have external dependencies:
local_exclude_app_modules += db_reg_agent
local_exclude_app_modules += gateway
local_exclude_app_modules += jsonrpc
local_exclude_app_modules += mp3
local_exclude_app_modules += py_sems
local_exclude_app_modules += twit

include $(COREPATH)/../Makefile.defs


# modules...

# if app_modules is set 
#   make $(app_modules)
# else 
#   if exclude_app_modules is set
#     make * - $(exclude_app_modules) + local excludes
#   else
#     make * - Makefile.defs:exclude_app_modules +  local excludes
#   

ifeq ($(app_modules),)
 exclude_modules = $(exclude_app_modules)
 exclude_modules += $(local_exclude_app_modules)

 ifneq ($(USE_MONITORING), yes)
 exclude_modules += monitoring
 endif

 $(info local_exclude_app_modules: $(local_exclude_app_modules))
 $(info exclude_app_modules: $(exclude_app_modules))
 app_modules = $(filter-out $(subst ;, ,$(exclude_modules)) \
		$(wildcard Makefile*) CVS CMakeLists.txt, \
		$(wildcard *) examples/tutorial/cc_acc )

endif


$(info app_modules: $(app_modules))

.PHONY: all
all: modules

.PHONY: clean
clean:
	@rm -f *.so
	@set -e ; \
	for r in $(app_modules) "" ; do \
		if [ -n "$$r" ]; then \
			echo "" ; \
			echo "Cleaning $$r" ; \
			echo "" ; \
			COREPATH=../$(COREPATH) $(MAKE) -C $$r clean ; \
		fi ; \
	done

.PHONY: modules
modules:
	@set -e ; \
	for r in $(app_modules) "" ; do \
		if [ -n "$$r" ]; then \
			echo  "" ; \
			echo "Making $$r" ; \
			echo  "" ; \
			COREPATH=../$(COREPATH) $(MAKE) -C $$r all; \
		fi ; \
	done 

.PHONY: install
install: install-bin install-cfg

.PHONY: install-bin
install-bin:
	@set -e ; \
	for r in $(app_modules) "" ; do \
		if [ -n "$$r" ]; then \
			echo "" ; \
			echo "Installing $$r binaries" ; \
			echo "" ; \
			COREPATH=../$(COREPATH) $(MAKE) -C $$r install; \
		fi ; \
	done

.PHONY: install-cfg
install-cfg: $(DESTDIR)$(cfg_target)
	@set -e ; \
	for r in $(app_modules) "" ; do \
		if [ -n "$$r" ]; then \
			echo "" ; \
			echo "Installing $$r configuration" ; \
			echo "" ; \
			COREPATH=../$(COREPATH) $(MAKE) -C $$r install-cfg; \
		fi ; \
	done
