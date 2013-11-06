APP_NAME ?= sems
NAME=$(APP_NAME)
LIBNAME=$(APP_NAME).so

COREPATH=.

PLUGIN_DIR=plug-in
SIP_STACK_DIR=sip
RESAMPLE_DIR=resample

SRCS=$(wildcard *.cpp)
HDRS=$(SRCS:.cpp=.h)
OBJS=$(SRCS:.cpp=.o)
DEPS=$(SRCS:.cpp=.d) $(NAME).d
AUDIO_FILES=$(notdir $(wildcard wav/*.wav))
TEST_DIR=tests

.PHONY: all
all: ../Makefile.defs $(NAME) modules

.PHONY: clean
clean:
	-@rm -f $(OBJS) $(DEPS) $(NAME)
	-@rm -f lib/*.so compat/getos compat/getarch
	-@$(MAKE) -C $(SIP_STACK_DIR) clean
	-@$(MAKE) -C $(RESAMPLE_DIR) clean
	-@$(MAKE) -C $(PLUGIN_DIR) clean

.PHONY: sip_stack
sip_stack:
	-@echo ""
	-@echo "making SIP stack"
	-@cd $(SIP_STACK_DIR); $(MAKE) all

.PHONY: modules
modules:
	-@echo ""
	-@echo "making core modules"
	-@cd $(PLUGIN_DIR); $(MAKE) modules

.PHONY: test
test:
	-@echo ""
	-@echo "making tests"
	-@cd $(TEST_DIR); $(MAKE)

.PHONY: core
core: $(OBJS) ../Makefile.defs

include ../Makefile.defs

CPPFLAGS += -I$(COREPATH) -fno-strict-aliasing
LDFLAGS += -levent -levent_pthreads

ifdef USE_LIBSAMPLERATE
CPPFLAGS += -DUSE_LIBSAMPLERATE
LDFLAGS +=-lsamplerate
endif

# This allows symbols defined in the SIP stack but not used
# by the core itself to be included in the executable and
# thus be available for modules
ifneq ($(LD), clang)
EXTRA_LDFLAGS += -Wl,--whole-archive $(SIP_STACK_DIR)/sip_stack.a -Wl,--no-whole-archive
else
EXTRA_LDFLAGS += -force_load $(SIP_STACK_DIR)/sip_stack.a
endif

ifneq ($(OS), freebsd)
	EXTRA_LDFLAGS += -lresolv
endif


ifdef USE_INTERNAL_RESAMPLER

CPPFLAGS += -DUSE_INTERNAL_RESAMPLER
OBJS += $(RESAMPLE_DIR)/libresample.a

.PHONY: resample_lib
resample_lib:
	-@echo ""
	-@echo "making internal resampler"
	-@cd $(RESAMPLE_DIR); $(MAKE) all

else

.PHONY: resample_lib
resample_lib:

endif

ifeq ($(USE_GPERFTOOLS),yes)
LDFLAGS += -ltcmalloc -lprofiler
endif

ifdef USE_SPANDSP
CPPFLAGS += -DUSE_SPANDSP -D__STDC_LIMIT_MACROS

ifdef LIBSPANDSP_STATIC
LDFLAGS += $(LIBSPANDSP_LDIR)libspandsp.a
else
LDFLAGS += -lspandsp
endif
endif

-include $(DEPS)

# implicit rules
%.o : %.cpp ../Makefile.defs
	$(CXX) -MMD -c -o $@ $< $(CPPFLAGS) $(CXXFLAGS)

$(NAME): sip_stack resample_lib core ../Makefile.defs
	-@echo ""
	-@echo "making $(NAME) executable"
	$(LD) -o $(NAME) $(OBJS) $(LDFLAGS) $(EXTRA_LDFLAGS)


install: all mk-install-dirs \
	install-audio \
	install-modules-cfg \
	install-cfg \
	install-bin \
	install-modules


# note: on solaris 8 sed: ? or \(...\)* (a.s.o) do not work
install-cfg: $(DESTDIR)$(cfg-target) $(DESTDIR)$(app-cfg-target)
		sed -e "s#/usr/.*lib/sems/audio/#$(audio-target)#g" \
			-e "s#/usr/.*lib/sems/plug-in/#$(modules-target)#g" \
			-e "s#/usr/.*etc/sems/#$(cfg-target)#g" \
			< etc/sems.conf.sample > $(DESTDIR)$(cfg-target)sems.conf.default
		chmod 644 $(DESTDIR)$(cfg-target)sems.conf.default
		if [ ! -f $(DESTDIR)$(cfg-target)sems.conf ]; then \
			cp -f $(DESTDIR)$(cfg-target)sems.conf.default \
				$(DESTDIR)$(cfg-target)sems.conf; \
		fi

		$(INSTALL-CFG) etc/app_mapping.conf.sample $(DESTDIR)$(app-cfg-target)app_mapping.conf

install-bin: $(DESTDIR)$(bin-prefix)/$(bin-dir)
		$(INSTALL-TOUCH) $(DESTDIR)$(bin-prefix)/$(bin-dir)$(NAME)
		$(INSTALL-BIN) $(NAME) $(DESTDIR)$(bin-prefix)/$(bin-dir)

install-modules: $(PLUGIN_DIR) $(DESTDIR)$(modules-prefix)/$(modules-dir)
	$(MAKE) -C $(PLUGIN_DIR) install

install-modules-cfg: $(PLUGIN_DIR) 
	$(MAKE) -C $(PLUGIN_DIR) install-cfg

install-audio: $(DESTDIR)$(audio-prefix)/$(audio-dir)
	for f in $(AUDIO_FILES) ; do \
		if [ -n "wav/$$f" ]; then \
			$(INSTALL-TOUCH) $(DESTDIR)$(audio-prefix)/$(audio-dir)$$f; \
			$(INSTALL-AUDIO) wav/$$f $(DESTDIR)$(audio-prefix)/$(audio-dir)$$f; \
		fi ; \
	done

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


