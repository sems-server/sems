plug_in_name = gateway

module_ldflags =-lmISDN -lisdnnet 
module_cflags  = 

COREPATH ?=../../core
module_cflags += -DGW_VERSION="\"0.1\""
# -I/usr/local/src/lcr/mISDN-2008_01_12/include -I/usr/local/src/lcr/mISDNuser-2008_01_12/include
include $(COREPATH)/plug-in/Makefile.app_module
