COREPATH ?=../../..
lib_name = libilbc.a

.PHONY: all
all: $(lib_name) 

include $(COREPATH)/../Makefile.defs


srcs     = $(wildcard *.c)
hrds     = $(wildcard *.h)
objs     = $(srcs:.c=.o) 
depends  = $(srcs:.c=.d)

AR		= ar
RANLIB		= ranlib


.PHONY: clean
clean:
	-@rm -f $(objs) $(depends) $(lib_name)

.PHONY: deps
deps: $(depends)

%.d: %.c %.h Makefile 
	$(CC) -MM $< $(CFLAGS) $(CPPFLAGS) > $@

%.o: %.c %.d
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(lib_name): deps $(objs)  Makefile
		if [ -f $(lib_name) ]; then rm $(RMFLAGS) $(lib_name) ; fi
		$(AR) $(ARFLAGS) $(lib_name) $(objs)
		$(RANLIB) $(lib_name)

ifeq ($(lib_name),$(MAKECMDGOALS))
include $(depends)
endif

