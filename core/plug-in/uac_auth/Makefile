COREPATH =../..
plug_in_name = uac_auth

#module_extra_objs = md5.o

#extra_depends = md5.d
#extra_clean = clean_md5

module_ldflags = 
module_cflags  = 

ifeq ($(OS),solaris)
	module_ldflags += -shared
	module_cflags += -shared
endif

#%.o : %.c %.d
#	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

#%.d : %.c %.h Makefile
#	$(CC) -MM $< $(CFLAGS) $(CPPFLAGS) > $@

#.PHONY: clean_md5
#clean_md5:
#	rm -f md5.o md5.d

include ../Makefile.app_module 
