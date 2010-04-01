LIBNAME=sip_stack.a

SRCS=$(wildcard *.cpp)
HDRS=$(SRCS:.cpp=.h)
OBJS=$(SRCS:.cpp=.o)
DEPS=$(SRCS:.cpp=.d)

CPPFLAGS += -I..

.PHONY: all
all: ../../Makefile.defs
	-@$(MAKE) deps    && \
	  $(MAKE) $(LIBNAME)


.PHONY: clean
clean:
	rm -f $(OBJS) $(DEPS) $(LIBNAME)

.PHONY: deps
deps: $(DEPS)

COREPATH=..
include ../../Makefile.defs

# implicit rules
%.o : %.cpp %.d ../../Makefile.defs
	$(CXX) -c -o $@ $< $(CPPFLAGS) $(CXXFLAGS)

%.d : %.cpp %.h ../../Makefile.defs
	$(CXX) -MM $< $(CPPFLAGS) $(CXXFLAGS) > $@

$(LIBNAME): $(OBJS) ../../Makefile.defs
	-@echo ""
	-@echo "making $(LIBNAME)"
	$(AR) rvs $(LIBNAME) $(OBJS)

ifeq '$(LIBNAME)' '$(MAKECMDGOALS)'
include $(DEPS)
endif


