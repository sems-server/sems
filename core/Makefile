NAME=sems
LIBNAME=sems.so

PLUGIN_DIR=plug-in
SRCS=$(filter-out $(NAME).cpp, $(wildcard *.cpp))
HDRS=$(SRCS:.cpp=.h)
OBJS=$(SRCS:.cpp=.o)
DEPS=$(SRCS:.cpp=.d) $(NAME).d
AUDIO_FILES=$(notdir $(wildcard wav/*.wav))


.PHONY: all
all:
	-@$(MAKE) deps    && \
	  $(MAKE) $(NAME) && \
	  $(MAKE) modules


.PHONY: clean
clean:
	rm -f $(OBJS) $(DEPS) $(NAME) $(NAME).o
	rm -f lib/*.so
	$(MAKE) -C $(PLUGIN_DIR) clean

.PHONY: modules
modules:
	$(MAKE) -C $(PLUGIN_DIR) $(MAKECMDGOALS)

.PHONY: deps
deps: $(DEPS)

.PHONY: doc
doc:
	rm -Rf doxygen_output; rm -rf ../doc/doxygen_doc ; \
	doxygen doxygen_proj ; mv doxygen_output ../doc/doxygen_doc

include ../Makefile.defs

# implicit rules
%.o : %.cpp %.d
	$(GPP) -c -o $@ $< $(CXX_FLAGS)

%.d : %.cpp %.h Makefile
	$(GPP) -MM $< $(CXX_FLAGS) > $@

$(NAME): $(NAME).o $(OBJS)
	$(GPP) -o $(NAME) $(NAME).o $(OBJS) $(LD_FLAGS)

install: all mk-install-dirs \
	install-cfg \
	install-bin \
	install-modules \
	install-doc \
	install-modules-cfg \
	install-audio \

# note: on solaris 8 sed: ? or \(...\)* (a.s.o) do not work
install-cfg: $(cfg-prefix)/$(cfg-dir)
		sed -e "s#/usr/.*lib/sems/audio/#$(audio-target)#g" \
			-e "s#/usr/.*lib/sems/plug-in/#$(modules-target)#g" \
			-e "s#/usr/.*etc/sems#$(cfg-target)#g" \
			< sems.conf.sample > $(cfg-prefix)/$(cfg-dir)sems.conf.default
		chmod 644 $(cfg-prefix)/$(cfg-dir)sems.conf.default
		if [ ! -f $(cfg-prefix)/$(cfg-dir)sems.conf ]; then \
			cp -f $(cfg-prefix)/$(cfg-dir)sems.conf.default \
				$(cfg-prefix)/$(cfg-dir)sems.conf; \
		fi

install-bin: $(bin-prefix)/$(bin-dir)
		$(INSTALL-TOUCH) $(bin-prefix)/$(bin-dir)$(NAME)
		$(INSTALL-BIN) $(NAME) $(bin-prefix)/$(bin-dir)

install-modules: $(PLUGIN_DIR) $(modules-prefix)/$(modules-dir)
	$(MAKE) -C $(PLUGIN_DIR) install

install-modules-cfg: $(PLUGIN_DIR) $(modules-prefix)/$(modules-dir)
	$(MAKE) -C $(PLUGIN_DIR) install-cfg

install-ser-cfg: etc/ser-sems.cfg
	mkdir -p $(ser-cfg-target)

	if [ ! -f  $(ser-cfg-target)/ser-sems.cfg ]; then \
		$(INSTALL-CFG) etc/ser-sems.cfg $(ser-cfg-target) ; \
	fi
	$(INSTALL-CFG) etc/ser-sems.cfg $(ser-cfg-target)ser-sems.cfg.default
	@echo \*\*\* installed ser-sems.cfg at $(ser-cfg-target)ser-sems.cfg
	@echo \*\*\* run $(ser-prefix)/sbin/ser -f $(ser-cfg-target)ser-sems.cfg
	@echo \*\*\* to start ser for sems.

install-audio: $(audio-prefix)/$(audio-dir)
	-@for f in $(AUDIO_FILES) ; do \
		if [ -n "wav/$$f" ]; then \
			$(INSTALL-TOUCH) $(audio-prefix)/$(audio-dir)$$f; \
			$(INSTALL-AUDIO) wav/$$f $(audio-prefix)/$(audio-dir)$$f; \
		fi ; \
	done

install-doc: $(doc-prefix)/$(doc-dir)
	$(INSTALL-TOUCH) $(doc-prefix)/$(doc-dir)README
	$(INSTALL-DOC) ../README $(doc-prefix)/$(doc-dir)

dist: tar

tar: 
	$(TAR) -C .. \
		--exclude=$(notdir $(CURDIR))/ivr \
		--exclude=$(notdir $(CURDIR))/tmp \
		--exclude=.svn* \
		--exclude=.\#* \
		--exclude=*.[do] \
		--exclude=*.la \
		--exclude=*.lo \
		--exclude=*.so \
		--exclude=*.il \
		--exclude=$(notdir $(CURDIR))/sems \
		--exclude=$(notdir $(CURDIR))/build-stamp \
		--exclude=$(notdir $(CURDIR))/configure-stamp \
		--exclude=$(notdir $(CURDIR))/debian/sems-core \
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
			                    -zcf ../../"$(NAME)-$(notdir $(CURDIR))-$(RELEASE)".tar.gz \
			                               "$(NAME)-$(RELEASE)" ) ; \
			    rm -rf tmp/_tar1; rm -rf tmp/_tar2

ifeq '$(NAME)' '$(MAKECMDGOALS)'
include $(DEPS)
endif


