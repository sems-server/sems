COREPATH ?=../../..
lib_name = libisac.a

exclude_sources = min_max_operations_neon.c

srcs     := $(filter-out $(exclude_sources), $(wildcard *.c))
hrds     := $(wildcard *.h)
objs     := $(srcs:.c=.o)
depends  := $(srcs:.c=.d)

AR	= ar
RANLIB	= ranlib
ARFLAGS	= rvs

.DEFAULT_GOAL := lib
.PHONY: lib
lib: $(lib_name)

.PHONY: clean
clean: $(extra_clean)
	@rm -f $(objs) $(depends) $(lib_name)

include $(COREPATH)/../Makefile.defs

-include $(depends)

%.o: %.c Makefile
	$(CC) -MMD $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(lib_name): $(objs)  Makefile
	if [ -f $(lib_name) ]; then rm $(RMFLAGS) $(lib_name) ; fi
	$(AR) $(ARFLAGS) $(lib_name) $(objs)
	$(RANLIB) $(lib_name)