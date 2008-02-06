COREPATH =../..
plug_in_name = sipctrl

module_ldflags = 
module_cflags  = -DMOD_NAME=\"$(plug_in_name)\"

%.o : %.c %.d
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

%.d : %.c %.h Makefile
	$(CC) -MM $< $(CFLAGS) $(CPPFLAGS) > $@

include ../Makefile.app_module 
