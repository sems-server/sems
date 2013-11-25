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

# RAW_IPHDR_INC_AUTO_FRAG (raw_sock.c):
# 	Fragmentation is done by the 
#	kernel (no need to do it in
#	userspace).
#
# RAW_IPHDR_IP_HBO (raw_sock.c):
# 	Byte order of ip offset and 
#       total length expected by the
#       kernel.
#
ifeq ($(OS), freebsd)
	CPPFLAGS += -DRAW_IPHDR_INC_AUTO_FRAG -DRAW_IPHDR_IP_HBO
else
ifeq ($(OS), openbsd)
	CPPFLAGS += -DRAW_IPHDR_INC_AUTO_FRAG
else
ifeq ($(OS), netbsd)
	CPPFLAGS += -DRAW_IPHDR_INC_AUTO_FRAG -DRAW_IPHDR_IP_HBO
else
ifeq ($(OS), macosx)
	CPPFLAGS += -DRAW_IPHDR_INC_AUTO_FRAG -DRAW_IPHDR_IP_HBO
endif
endif
endif
endif

# implicit rules
%.o : %.cpp $(COREPATH)/../Makefile.defs
	$(CXX) -MMD -c -o $@ $< $(CPPFLAGS) $(CXXFLAGS)

$(LIBNAME): $(OBJS) $(COREPATH)/../Makefile.defs
	-@echo ""
	-@echo "making $(LIBNAME)"
	$(AR) rvs $(LIBNAME) $(OBJS)

-include $(DEPS)


