NAME=sems

.DEFAULT_GOAL:=all

.PHONY: all
all: modules

COREPATH=core
include Makefile.defs

subdirs = core apps

.PHONY: clean
clean:
	-@rm -f *.so
	-@for r in $(subdirs) "" ; do \
		if [ -n "$$r" ]; then \
			echo "" ; \
			echo "making $$r" ; \
			$(MAKE) -C $$r clean ; \
		fi ; \
	done

.PHONY: modules
modules:
	-@for r in $(subdirs) "" ; do \
		if [ -n "$$r" ]; then \
			echo  "" ; \
			echo  "making $$r" ; \
			$(MAKE) -C $$r all; \
		fi ; \
	done 

.PHONY: install
install:
	-@for r in $(subdirs) "" ; do \
		if [ -n "$$r" ]; then \
			echo "" ; \
			echo "" ; \
			$(MAKE) -C $$r install; \
		fi ; \
	done
	@echo ""
	@echo "*** install complete. Run SEMS with "
	@echo "*** "
	@echo "***   $(bin-target)sems -f $(cfg-target)sems.conf"


.PHONY: dist
dist: tar

.PHONY: tar
tar: 
	$(TAR) -C .. \
		--exclude=$(notdir $(CURDIR))/tmp \
		--exclude=core/$(notdir $(CURDIR))/tmp \
		--exclude=.svn* \
		--exclude=.\#* \
		--exclude=*.[do] \
		--exclude=*.la \
		--exclude=*.lo \
		--exclude=*.so \
		--exclude=*.il \
		--exclude=*.gz \
		--exclude=*.bz2 \
		--exclude=*.tar \
		--exclude=*~ \
		-cf - $(notdir $(CURDIR)) | \
			(mkdir -p tmp/_tar1; mkdir -p tmp/_tar2 ; \
			    cd tmp/_tar1; $(TAR) -xf - ) && \
			    mv tmp/_tar1/$(notdir $(CURDIR)) \
			       tmp/_tar2/"$(NAME)-$(REL_VERSION)" && \
			    (cd tmp/_tar2 && $(TAR) \
			                    -zcf ../../"$(NAME)-$(RELEASE)".tar.gz \
			                               "$(NAME)-$(REL_VERSION)" ) ; \
			    rm -rf tmp

.PHONY: doc
doc:
	make -C core/ doc

.PHONY: fulldoc
fulldoc:
	make -C core/ fulldoc
