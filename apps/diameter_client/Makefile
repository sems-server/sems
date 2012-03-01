plug_in_name = diameter_client

DIAMETER_BASE_LIBDIR = lib_dbase/
DIAMETER_BASE_LIBNAME = lib_dbase.a

# TLS support, mandatory
WITH_OPENSSL = 1

module_ldflags = 
module_cflags  = -I $(DIAMETER_BASE_LIBDIR)


ifdef WITH_OPENSSL
module_ldflags += -lssl -lcrypto
#module_cflags += -DWITH_OPENSSL
endif

module_extra_objs = $(DIAMETER_BASE_LIBDIR)$(DIAMETER_BASE_LIBNAME)
extra_clean = baseclean

COREPATH ?=../../core
include $(COREPATH)/plug-in/Makefile.app_module

$(DIAMETER_BASE_LIBDIR)$(DIAMETER_BASE_LIBNAME):
	$(MAKE) -C $(DIAMETER_BASE_LIBDIR) 

baseclean:
	$(MAKE) -C $(DIAMETER_BASE_LIBDIR) clean
