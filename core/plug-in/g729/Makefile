plug_in_name = g729
COREPATH =../..

# adapt these paths to version/arch
IPPROOT?=/opt/intel/ipp/6.1.2.051/ia32/
IPP_SAMPLES_PATH?=/opt/intel/ipp/ipp-samples/
IPP_SAMPLES_ARCH?=linux32_gcc4

######

IPP_LIB_PATH=$(IPPROOT)sharedlib/

IPP_INC = -I$(IPP_SAMPLES_PATH)speech-codecs/core/usc/include 
IPP_LIB ?= -L$(IPP_SAMPLES_PATH)speech-codecs/_bin/$(IPP_SAMPLES_ARCH)/lib -lspeech -lcore \
	  -L$(IPP_LIB_PATH) -lippsc -lipps

module_ldflags = $(IPP_LIB)
module_cflags  = $(IPP_INC)

ifdef NOFPU
	module_cflags += -DNOFPU
endif

include ../Makefile.audio_module
