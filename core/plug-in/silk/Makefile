plug_in_name = silk
COREPATH =../..

SILK_DIR=SILK_SDK_SRC_v1.0.8/SILK_SDK_SRC_FIX_v1.0.8
SILK_INC=$(SILK_DIR)/interface
SILK_LIB = $(SILK_DIR)/libSKP_SILK_SDK.a

module_cflags = -I $(SILK_INC)
module_extra_objs = $(SILK_LIB)
extra_clean = clean_silk

include ../Makefile.audio_module

$(SILK_LIB):
	$(MAKE) -C $(SILK_DIR) lib

.PHONY: clean_silk
clean_silk:
	$(MAKE) -C $(SILK_DIR) clean
