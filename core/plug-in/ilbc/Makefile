COREPATH =../..

plug_in_name = ilbc
ILBC_DIR = ./iLBC_rfc3951

module_ldflags = -lm -fPIC 
module_extra_objs = $(ILBC_DIR)/libilbc.a
module_cflags  = -I$(ILBC_DIR) 

extra_clean = clean_ilbclib

include ../Makefile.audio_module

$(ILBC_DIR)/libilbc.a:
	$(MAKE) -C $(ILBC_DIR) 

.PHONY: clean_ilbclib
clean_ilbclib:
	$(MAKE) -C $(ILBC_DIR) clean
