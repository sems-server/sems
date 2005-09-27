
lib_name = libilbc.a

srcs     = $(wildcard *.c)
hrds     = $(wildcard *.h)
objs     = $(srcs:.c=.o) 
depends  = $(srcs:.c=.d)

AR		= ar
RANLIB		= ranlib

.PHONY: all
all: $(lib_name) 

.PHONY: clean
clean:
	-@rm -f $(objs) $(depends) $(lib_name)

.PHONY: deps
deps: $(depends)

%.d: %.c Makefile 
	gcc -MM $< $(cflags) > $@

%.o: %.c %.d
	gcc $(cflags) -c $< -o $@ -fPIC

$(lib_name): $(objs)  Makefile
		-rm $(RMFLAGS) $(lib_name)
		$(AR) $(ARFLAGS) $(lib_name) $(objs)
		$(RANLIB) $(lib_name)

ifeq ($(lib_name),$(MAKECMDGOALS))
include $(depends)
endif

