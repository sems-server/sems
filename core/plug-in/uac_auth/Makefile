COREPATH =../..
plug_in_name = uac_auth

module_extra_objs = md5.o

extra_depends = md5.d
extra_clean	  = md5.o md5.d

module_ldflags = 
module_cflags  = 

%.o : %.c %.d
	$(CC) $(cflags) -c $< -o $@

%.d : %.c %.h Makefile
	$(CC) -MM $< $(cflags) > $@


include ../Makefile.app_module 
