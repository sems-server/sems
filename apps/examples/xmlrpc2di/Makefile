XMLRPCPP_DIR = xmlrpc++0.7

plug_in_name = xmlrpc2di

module_ldflags =
module_cflags  = -I$(XMLRPCPP_DIR)/src 

module_extra_objs = $(XMLRPCPP_DIR)/libXmlRpc.a

extra_clean = clean_libxmlrpc


COREPATH ?=../../../core
include $(COREPATH)/plug-in/Makefile.app_module

$(XMLRPCPP_DIR)/libXmlRpc.a: 
	$(MAKE) -C $(XMLRPCPP_DIR) libXmlRpc.a

clean_libxmlrpc: 
	@if [ -d $(XMLRPCPP_DIR) ]; \
		then $(MAKE) -C $(XMLRPCPP_DIR) clean; \
	fi

install-xmlrpcpp: xmlrpc++0.7 xmlrpc++0.7/.sems_patched

xmlrpc++0.7/.sems_patched: xmlrpc++0.7
	if ![ -f xmlrpc++0.7/.sems_patched ]; \
	 then  patch -p0 < xmlrpcpp07_sems.patch; \
	fi
	touch xmlrpc++0.7/.sems_patched

xmlrpc++0.7: xmlrpc++0.7.tar.gz
	$(TAR) xzvf xmlrpc++0.7.tar.gz

xmlrpc++0.7.tar.gz:
	wget http://switch.dl.sourceforge.net/sourceforge/xmlrpcpp/xmlrpc++0.7.tar.gz

