plug_in_name = speex

SPEEX_INC?=/usr/include/
SPEEX_LIB?=-lspeex

# or: 
# SPEEXDIR?=speex_src
# SPEEX_INC?=$(SPEEXDIR)
# SPEEX_LIB?=$(SPEEXDIR)/libspeex/.libs/libspeex.a
#
# CONFIGURE_FLAG=--with-pic


module_ldflags = $(SPEEX_LIB)
module_cflags  = -I $(SPEEX_INC) -ansi # -DNOFPU

# or
# module_extra_objs = $(SPEEX_LIB)
#extra_clean = clean_speex

include ../Makefile.audio_module


#$(SPEEXDIR)/Makefile:
#	cd $(SPEEXDIR); ./configure $(CONFIGURE_FLAG)
#
#$(SPEEXDIR)/libspeex/.libs/libspeex.a:: $(SPEEXDIR)/Makefile
#	$(MAKE) -C $(SPEEXDIR)/libspeex
#
#.PHONY: clean_speex
#clean_speex:
#	$(MAKE) -C $(SPEEXDIR) distclean
