XMLRPCPP_DIR = xmlrpc++0.7

plug_in_name = xmlrpc2di

module_ldflags = -lxmlrpc++ 
module_cflags  = 
# for gentoo ebuild or cvs-20040713 version:
# module_cflags  = -DHAVE_XMLRPCPP_SSL
#

# use these for local installation: 
# module_cflags  = -I$(XMLRPCPP_DIR)/src 
# module_extra_objs = $(XMLRPCPP_DIR)/libXmlRpc.a
# extra_clean = clean_libxmlrpc
# and comment module_ldflags line above


COREPATH ?=../../core
include $(COREPATH)/plug-in/Makefile.app_module

#$(XMLRPCPP_DIR)/libXmlRpc.a: 
#	$(MAKE) -C $(XMLRPCPP_DIR) libXmlRpc.a
#
#clean_libxmlrpc: 
#	@if [ -d $(XMLRPCPP_DIR) ]; \
#		then $(MAKE) -C $(XMLRPCPP_DIR) clean; \
#	fi
#
#install-xmlrpcpp: xmlrpc++0.7
#
#xmlrpc++0.7: xmlrpc++0.7.tar.gz
#	$(TAR) xzvf xmlrpc++0.7.tar.gz
#	patch -p0 < xmlrpcpp07_sems.patch
#
#xmlrpc++0.7.tar.gz:
#	wget http://switch.dl.sourceforge.net/sourceforge/xmlrpcpp/xmlrpc++0.7.tar.gz

