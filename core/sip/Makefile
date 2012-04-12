LIBNAME=sip_stack.a

SRCS=$(wildcard *.cpp)
HDRS=$(SRCS:.cpp=.h)
OBJS=$(SRCS:.cpp=.o)
DEPS=$(SRCS:.cpp=.d)

.PHONY: all
all: $(LIBNAME)

.PHONY: clean
clean:
	rm -f $(OBJS) $(DEPS) $(LIBNAME)

COREPATH = ..
include $(COREPATH)/../Makefile.defs

CPPFLAGS += -I$(COREPATH) -fno-strict-aliasing

# implicit rules
%.o : %.cpp $(COREPATH)/../Makefile.defs
	$(CXX) -MMD -c -o $@ $< $(CPPFLAGS) $(CXXFLAGS)

$(LIBNAME): $(OBJS) $(COREPATH)/../Makefile.defs
	-@echo ""
	-@echo "making $(LIBNAME)"
	$(AR) rvs $(LIBNAME) $(OBJS)

-include $(DEPS)


