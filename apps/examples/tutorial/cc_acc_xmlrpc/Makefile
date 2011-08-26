###########################################################
XMLRPCPP_DIR = ../../../xmlrpc2di/xmlrpc++

plug_in_name = cc_acc_xmlrpc

module_ldflags = -lssl
module_cflags  = -I$(XMLRPCPP_DIR)/src

module_extra_objs = $(XMLRPCPP_DIR)/libXmlRpc.a

extra_clean = clean_libxmlrpc


COREPATH ?=../../../../core
include $(COREPATH)/plug-in/Makefile.app_module

$(XMLRPCPP_DIR)/libXmlRpc.a:
		$(MAKE) -C $(XMLRPCPP_DIR) libXmlRpc.a

clean_libxmlrpc:
		$(MAKE) -C $(XMLRPCPP_DIR) clean

###############################################################
