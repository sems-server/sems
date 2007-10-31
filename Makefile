NAME=sems

.DEFAULT_GOAL:=all

.PHONY: all
all: modules

include Makefile.defs

modules = core apps $(wildcard ser-0.9.6-sems*)  
imodules = core apps

# or, if you want to build all that is there:
# modules = $(filter-out $(wildcard Makefile* README doc *gz), \
#			$(wildcard *) ) 
# imodules = $(filter-out ser-0.9.6-sems, $(modules))

.PHONY: clean
clean:
	-@rm -f *.so
	-@for r in $(modules) "" ; do \
		if [ -n "$$r" ]; then \
			echo "" ; \
			echo "" ; \
			$(MAKE) -C $$r clean ; \
		fi ; \
	done

.PHONY: modules
modules:
	-@for r in $(modules) "" ; do \
		if [ -n "$$r" ]; then \
			echo  "" ; \
			echo  "" ; \
			$(MAKE) -C $$r all; \
		fi ; \
	done 

.PHONY: install
install:
	-@for r in $(imodules) "" ; do \
		if [ -n "$$r" ]; then \
			echo "" ; \
			echo "" ; \
			$(MAKE) -C $$r install; \
		fi ; \
	done
	-@if [ -d ser-0.9.6-sems ]; then \
			echo "" ;\
			echo "making install in ser-0.9.6" ;\
			echo "using PREFIX=$(SERPREFIX)" ;\
			echo "" ;\
			$(MAKE) -C ser-0.9.6-sems install PREFIX=$(SERPREFIX) ;\
			echo "" ;\
			echo "installing ser-sems.cfg" ;\
			$(MAKE) -C . install-ser-cfg ;\
	fi
	@echo ""
	@echo "*** install complete. Run SEMS with "
	@echo "*** "
	@echo "***   $(bin-target)sems -f $(cfg-target)sems.conf"

	-@if [ -d ser-0.9.6-sems ]; then \
		echo "*** "; \
		echo "*** and"; \
		echo "***"; \
		echo "***   $(ser-prefix)/sbin/ser -f $(ser-cfg-target)ser-sems.cfg" ;\
		echo "***"; \
	fi



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
			       tmp/_tar2/"$(NAME)-$(RELEASE)" && \
			    (cd tmp/_tar2 && $(TAR) \
			                    -zcf ../../"$(NAME)-$(RELEASE)".tar.gz \
			                               "$(NAME)-$(RELEASE)" ) ; \
			    rm -rf tmp

ser-0.9.6-sems_src.tar.gz:
	wget http://ftp.iptel.org/pub/sems/ser-0.9.6-sems_src.tar.gz

ser-0.9.6-sems:	ser-0.9.6-sems_src.tar.gz
	tar xzvf ser-0.9.6-sems_src.tar.gz

.PHONY: bundle
bundle: ser-0.9.6-sems tar
	mv "$(NAME)-$(RELEASE)".tar.gz "$(NAME)-$(RELEASE)"-bundle.tar.gz

.PHONY: doc
doc:
	make -C core/ doc

.PHONY: fulldoc
fulldoc:
	make -C core/ fulldoc

.PHONY: install-ser-cfg
install-ser-cfg:
	make -C core/ install-ser-cfg
