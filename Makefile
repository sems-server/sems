.DEFAULT_GOAL:=all

.PHONY: all
all: modules

COREPATH=core
include Makefile.defs

NAME=$(APP_NAME)

subdirs = core apps tools

.PHONY: clean
clean:
	@rm -f *.so
	@set -e ; \
	for r in $(subdirs) doc "" ; do \
		if [ -n "$$r" ]; then \
			echo "" ; \
			echo "making $$r" ; \
			$(MAKE) -C $$r clean ; \
		fi ; \
	done

.PHONY: modules
modules:
	@set -e ; \
	for r in $(subdirs) "" ; do \
		if [ -n "$$r" ]; then \
			echo  "" ; \
			echo  "making $$r" ; \
			$(MAKE) -C $$r all; \
		fi ; \
	done 

.PHONY: install
install:
	@set -e ; \
	for r in $(subdirs) "" ; do \
		if [ -n "$$r" ]; then \
			echo "" ; \
			echo "" ; \
			$(MAKE) -C $$r install; \
		fi ; \
	done
	@echo ""
	@echo "*** install complete. Run SEMS with "
	@echo "*** "
	@echo "***   $(bin_target)$(NAME) -f $(cfg_target)sems.conf"


.PHONY: dist
dist: tar

.PHONY: tar
tar: 
	$(TAR) -C .. \
		--exclude=$(notdir $(CURDIR))/tmp \
		--exclude=core/$(notdir $(CURDIR))/tmp \
		--exclude=.svn* \
		--exclude=.git* \
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
			       tmp/_tar2/"$(NAME)-$(RELEASE)" && \
			    (cd tmp/_tar2 && $(TAR) \
			                    -zcf ../../"$(NAME)-$(RELEASE)".tar.gz \
			                               "$(NAME)-$(RELEASE)" ) ; \
			    rm -rf tmp

# the rpmtar target creates source tar.gz file, with versions taken from rpm spec file
# the tarball can be used for rpm building 
.PHONY: rpmtar
rpmtar: 
	rm -rf /tmp/_tar1
	rm -rf /tmp/_tar2
	rm -rf "/root/rpmbuild/SOURCES/$(NAME)-*.tar.gz"
	$(TAR) -C .. \
	--exclude=$(notdir $(CURDIR))/tmp \
	--exclude=core/$(notdir $(CURDIR))/tmp \
                --exclude=.svn* \
                --exclude=.git* \
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
                        (mkdir -p /tmp/_tar1; mkdir -p /tmp/_tar2 ; \
                            cd /tmp/_tar1; $(TAR) -xf - ) && \
                            mv /tmp/_tar1/$(notdir $(CURDIR)) \
                               /tmp/_tar2/"$(NAME)-$(RELEASE)" && \
                            (cd /tmp/_tar2 && $(TAR) \
                                            -zcf /root/rpmbuild/SOURCES/"$(NAME)-$(RELEASE)".tar.gz \
                                                       "$(NAME)-$(RELEASE)" ) ; \
                            rm -rf /tmp/_tar1 /tmp/_tar2;
	ls -al /root/rpmbuild/SOURCES/$(NAME)-*.tar.gz

.PHONY: doc
doc:
	make -C doc/ doc

.PHONY: fulldoc
fulldoc:
	make -C doc/ fulldoc
