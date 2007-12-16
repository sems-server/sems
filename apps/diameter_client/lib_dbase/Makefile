lib_name = lib_dbase.a

.PHONY: all
all: $(lib_name) 

MCOREPATH ?=../../../core
include $(MCOREPATH)/../Makefile.defs


srcs     = $(wildcard *.c)
hrds     = $(wildcard *.h)
objs     = $(srcs:.c=.o) 
depends  = $(srcs:.c=.d)

cflags  ?= $(CFLAGS) -I $(MCOREPATH) -Wall $(module_cflags)

AR		= ar
RANLIB		= ranlib


.PHONY: clean
clean:
	-@rm -f $(objs) $(depends) $(lib_name)

.PHONY: deps
deps: $(depends)

%.d: %.c %.h Makefile 
	$(CC) -MM $< $(cflags) > $@

%.o: %.c %.d
	$(CC) $(cflags) -c $< -o $@ -fPIC

$(lib_name): deps $(objs)  Makefile
		if [ -f $(lib_name) ]; then rm $(RMFLAGS) $(lib_name) ; fi
		$(AR) $(ARFLAGS) $(lib_name) $(objs)
		$(RANLIB) $(lib_name)

ifeq ($(lib_name),$(MAKECMDGOALS))
include $(depends)
endif

