plug_in_name = cc_bl_redis
sbc_app_path = ../..

#HIREDIS_DIR?=hiredis/
#module_extra_objs = $(HIREDIS_DIR)libhiredis.a
#module_cflags  = -DMOD_NAME=\"$(plug_in_name)\" -I$(sbc_app_path) -I$(HIREDIS_DIR)

module_ldflags = -lhiredis
module_cflags  = -DMOD_NAME=\"$(plug_in_name)\" -I$(sbc_app_path)

COREPATH =../../../../core
include $(COREPATH)/plug-in/Makefile.app_module

$(HIREDIS_DIR)libhiredis.a: $(HIREDIS_DIR)
	$(SH) $(MAKE) -C $(HIREDIS_DIR) libhiredis.a
