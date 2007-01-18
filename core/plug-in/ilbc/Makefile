COREPATH =../..

plug_in_name = ilbc
ILBC_DIR = ./iLBC_rfc3951

module_ldflags = -lm -fPIC 
module_extra_objs = $(ILBC_DIR)/libilbc.a
# module_extra_objs =  $(ILBC_DIR)/anaFilter.o \
# 			$(ILBC_DIR)/constants.o \
# 			$(ILBC_DIR)/createCB.o \
# 			$(ILBC_DIR)/doCPLC.o \
# 			$(ILBC_DIR)/enhancer.o \
# 			$(ILBC_DIR)/filter.o \
# 			$(ILBC_DIR)/FrameClassify.o \
# 			$(ILBC_DIR)/gainquant.o \
# 			$(ILBC_DIR)/getCBvec.o \
# 			$(ILBC_DIR)/helpfun.o \
# 			$(ILBC_DIR)/hpInput.o \
# 			$(ILBC_DIR)/hpOutput.o \
# 			$(ILBC_DIR)/iCBConstruct.o \
# 			$(ILBC_DIR)/iCBSearch.o \
# 			$(ILBC_DIR)/iLBC_decode.o \
# 			$(ILBC_DIR)/iLBC_encode.o \
# 			$(ILBC_DIR)/LPCdecode.o \
# 			$(ILBC_DIR)/LPCencode.o \
# 			$(ILBC_DIR)/lsf.o \
# 			$(ILBC_DIR)/packing.o \
# 			$(ILBC_DIR)/StateConstructW.o \
# 			$(ILBC_DIR)/StateSearchW.o \
# 			$(ILBC_DIR)/syntFilter.o

module_cflags  = -I$(ILBC_DIR) 

extra_clean = clean_ilbclib

include ../Makefile.audio_module

$(ILBC_DIR)/libilbc.a::
	-@cd $(ILBC_DIR); $(MAKE) all

.PHONY: clean_ilbclib
clean_ilbclib:
	$(MAKE) -C $(ILBC_DIR) clean
