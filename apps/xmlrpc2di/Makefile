XMLRPCPP_DIR = xmlrpc++

plug_in_name = xmlrpc2di

#module_ldflags = -lxmlrpc++ 
#module_cflags  = 
# for gentoo ebuild or cvs-20040713 version:
# module_cflags  = -DHAVE_XMLRPCPP_SSL
#
# if your xmlrpcpp version has access to struct value map 
#(    operator XmlRpcValue::ValueStruct const&() in XmlRpcValue.h around line 168)
# you can support struct type in XMLRPC conversion 
# -D XMLRPCPP_SUPPORT_STRUCT_ACCESS

# use these for local installation: 
 module_ldflags = -lssl
 module_cflags  = -I$(XMLRPCPP_DIR)/src -DHAVE_XMLRPCPP_SSL -D XMLRPCPP_SUPPORT_STRUCT_ACCESS
 module_extra_objs = $(XMLRPCPP_DIR)/libXmlRpc.a
 extra_clean = clean_libxmlrpc
# and comment module_ldflags line above


COREPATH ?=../../core
include $(COREPATH)/plug-in/Makefile.app_module

$(XMLRPCPP_DIR)/libXmlRpc.a: 
	$(MAKE) -C $(XMLRPCPP_DIR) libXmlRpc.a ARFLAGS=rvs
clean_libxmlrpc: 
	@if [ -d $(XMLRPCPP_DIR) ]; \
		then $(MAKE) -C $(XMLRPCPP_DIR) clean; \
	fi
