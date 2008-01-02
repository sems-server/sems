

include ../Makefile.defs

LIBNAME=lib${NAME}

CFLAGS+=-D_LIBBINRPC_BUILD

.PHONY: all build
all build: dep $(OBJ) $(EXTRA_DEPS)
	$(AR) $(ARFLAGS) $(LIBNAME).a $(OBJ)
#	$(CC) $(OBJ) $(LDFLAGS) $(LDLIBS) -o $(LIBNAME).so
	$(LD) $(LDFLAGS) $(LDLIBS) -o $(LIBNAME).so $(OBJ)

.PHONY: deps
dep: $(DEP)

ifeq (,$(findstring clean,${MAKECMDGOALS}))
-include $(OBJ:.o=.d)
endif

.PHONY: clean
clean:
	rm -f *~ $(OBJ) $(DEP) $(LIBNAME).a $(LIBNAME).so

