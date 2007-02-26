COREPATH ?=../../..
include $(COREPATH)/../Makefile.defs

lib_name = libilbc.a

srcs     = $(wildcard *.c)
hrds     = $(wildcard *.h)
objs     = $(srcs:.c=.o) 
depends  = $(srcs:.c=.d)

cflags ?= $(C_FLAGS)

AR		= ar
RANLIB		= ranlib

.PHONY: all
all: $(lib_name) 

.PHONY: clean
clean:
	-@rm -f $(objs) $(depends) $(lib_name)

.PHONY: deps
deps: $(depends)

%.d: %.c %.h Makefile 
	gcc -MM $< $(cflags) > $@

%.o: %.c %.d
	gcc $(cflags) -c $< -o $@ -fPIC

$(lib_name): deps $(objs)  Makefile
		if [ -f $(lib_name) ]; then rm $(RMFLAGS) $(lib_name) ; fi
		$(AR) $(ARFLAGS) $(lib_name) $(objs)
		$(RANLIB) $(lib_name)

ifeq ($(lib_name),$(MAKECMDGOALS))
include $(depends)
endif

