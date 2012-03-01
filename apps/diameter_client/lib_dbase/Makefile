lib_name = lib_dbase.a

.PHONY: all
all: $(lib_name) 

COREPATH = ../../../core
include $(COREPATH)/../Makefile.defs

srcs     = $(wildcard *.c)
hrds     = $(wildcard *.h)
objs     = $(srcs:.c=.o) 
depends  = $(srcs:.c=.d)

cflags  ?= $(CPPFLAGS) $(CFLAGS) -I $(COREPATH)

WITH_OPENSSL = 1

ifdef WITH_OPENSSL
cflags+=  -D WITH_OPENSSL
endif

AR		= ar
RANLIB		= ranlib


.PHONY: clean
clean:
	-@rm -f $(objs) $(depends) $(lib_name)

-include $(depends)

%.o: %.c Makefile
	$(CC) -MMD $(cflags) -c $< -o $@ -fPIC

$(lib_name): $(objs)  Makefile
		if [ -f $(lib_name) ]; then rm $(RMFLAGS) $(lib_name) ; fi
		$(AR) $(ARFLAGS) $(lib_name) $(objs)
		$(RANLIB) $(lib_name)
