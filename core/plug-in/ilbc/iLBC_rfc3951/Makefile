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

-include $(depends)

%.o: %.c Makefile
	$(CC) -MMD $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(lib_name): $(objs)  Makefile
		if [ -f $(lib_name) ]; then rm $(RMFLAGS) $(lib_name) ; fi
		$(AR) $(ARFLAGS) $(lib_name) $(objs)
		$(RANLIB) $(lib_name)


