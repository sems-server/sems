
plug_in_name = rtmp

module_cflags  = -DNO_CRYPTO
module_ldflags = -lspeex
module_extra_objs = librtmp/librtmp.a

extra_target = sono
extra_clean = clean_sono clean_librtmp

COREPATH ?=../../core
include $(COREPATH)/plug-in/Makefile.app_module

#CXXFLAGS=-g -I/opt/local/include -DNO_CRYPTO
#LDFLAGS=-lstdc++ -g -L/opt/local/lib -lspeex -lpthread

.PHONY: librtmp/librtmp.a
librtmp/librtmp.a:
	cd librtmp; $(MAKE)

.PHONY: sono
sono:
	cd flash_phone; $(MAKE)

.PHONY: clean_sono
clean_sono:
	cd flash_phone; $(MAKE) clean

.PHONY: clean_librtmp
clean_librtmp:
	cd librtmp; $(MAKE) clean
