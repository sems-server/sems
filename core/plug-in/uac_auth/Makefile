plug_in_name = uac_auth

module_extra_objs = md5.o

module_ldflags = 
module_cflags  = 

#md5.o: md5.c md5.d
#	$(CC) $(cflags) -c $< -o $@
#
#md5.d: md5.c md5.h Makefile
#	$(CC) -MM $< $(cflags) > $@

%.o : %.c %.d
	$(CC) $(cflags) -c $< -o $@

%.d : %.c %.h Makefile
	$(CC) -MM $< $(cflags) > $@

include ../Makefile.app_module
