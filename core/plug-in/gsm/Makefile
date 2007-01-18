COREPATH =../..

plug_in_name = gsm

module_ldflags = 
module_cflags  = -I gsm-1.0-pl10/inc -DHAS_STDLIB_H

module_extra_objs = gsm-1.0-pl10/lib/libgsm.a

extra_clean = clean_gsmlib

include ../Makefile.audio_module

gsm-1.0-pl10/lib/libgsm.a::
	cd gsm-1.0-pl10; $(MAKE) ./lib/libgsm.a

.PHONY: clean_gsmlib
clean_gsmlib:
	cd gsm-1.0-pl10; $(MAKE) clean
