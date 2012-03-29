plug_in_name = isac
COREPATH =../..

ISAC_DIR = libisac
ISAC_LIB = $(ISAC_DIR)/libisac.a

module_extra_objs = $(ISAC_LIB)
extra_clean = clean_isac

include ../Makefile.audio_module

$(ISAC_LIB):
	$(MAKE) -C $(ISAC_DIR) lib

.PHONY: clean_isac
clean_isac:
	$(MAKE) -C $(ISAC_DIR) clean
