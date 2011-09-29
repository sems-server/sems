plug_in_name = cc_prepaid_xmlrpc
sbc_app_path = ../..
COREPATH =../../../../core

XMLRPCPP_DIR = $(COREPATH)/../apps/xmlrpc2di/xmlrpc++

module_ldflags = -lssl
module_cflags  = -DMOD_NAME=\"$(plug_in_name)\"  -I$(sbc_app_path) -I$(XMLRPCPP_DIR)/src

module_extra_objs = $(XMLRPCPP_DIR)/libXmlRpc.a

extra_clean = clean_libxmlrpc
include $(COREPATH)/plug-in/Makefile.app_module


$(XMLRPCPP_DIR)/libXmlRpc.a:
		$(MAKE) -C $(XMLRPCPP_DIR) libXmlRpc.a

clean_libxmlrpc:
		$(MAKE) -C $(XMLRPCPP_DIR) clean

