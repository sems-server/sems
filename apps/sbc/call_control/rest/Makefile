plug_in_name = cc_rest
sbc_app_path = ../..

# for libcurl version 7.17.1 works this:
curl_prefix   = `curl-config --prefix`
curl_libs     = `curl-config --libs`   # -L$(curl_prefix)/lib
curl_cflags   = `curl-config --cflags` # -I$(curl_prefix)/includes

module_ldflags = $(curl_libs)
module_cflags  = -DMOD_NAME=\"$(plug_in_name)\"  -I$(sbc_app_path) $(curl_cflags)

COREPATH =../../../../core
include $(COREPATH)/plug-in/Makefile.app_module
