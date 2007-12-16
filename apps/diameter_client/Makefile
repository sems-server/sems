plug_in_name = diameter_client

DIAMETER_BASE_LIBDIR = lib_dbase/
DIAMETER_BASE_LIBNAME = lib_dbase.a

module_ldflags =
module_cflags  = -I $(DIAMETER_BASE_LIBDIR)

module_extra_objs = $(DIAMETER_BASE_LIBDIR)$(DIAMETER_BASE_LIBNAME)
extra_clean = baseclean

COREPATH ?=../../core
include $(COREPATH)/plug-in/Makefile.app_module

$(DIAMETER_BASE_LIBDIR)$(DIAMETER_BASE_LIBNAME):
	make -C $(DIAMETER_BASE_LIBDIR) $(DIAMETER_BASE_LIBNAME)

baseclean:
	make -C $(DIAMETER_BASE_LIBDIR) clean
