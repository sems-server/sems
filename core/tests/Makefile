NAME=sems_tests

SIP_STACK_DIR=../sip
SIP_STACK=$(SIP_STACK_DIR)/sip_stack.a
CORE_SRCS=$(filter-out ../sems.cpp , $(wildcard ../*.cpp))
CORE_HDRS=$(CORE_SRCS:.cpp=.h)
CORE_OBJS=$(CORE_SRCS:.cpp=.o)
CORE_DEPS=$(subst ../,,$(CORE_SRCS:.cpp=.d))

SRCS=$(wildcard *.cpp)
HDRS=$(SRCS:.cpp=.h)
OBJS=$(SRCS:.cpp=.o)
DEPS=$(SRCS:.cpp=.d)

CPPFLAGS += -I.. -DNOMAIN

EXTRA_LDFLAGS += -lresolv

.PHONY: all
all: ../../Makefile.defs
	-@$(MAKE) core_deps  && $(MAKE) deps    && \
	  $(MAKE) $(NAME) && \
	./$(NAME)

.PHONY: sip_stack
sip_stack:
	-@echo ""
	-@echo "making SIP stack"
	-@cd $(SIP_STACK_DIR); $(MAKE) all

SIP_STACK: sip_stack

.PHONY: clean
clean:
	rm -f $(OBJS) $(DEPS) $(CORE_DEPS) $(CORE_OBJS) $(NAME)

.PHONY: deps
deps: $(DEPS)

.PHONY: core_deps
core_deps: $(CORE_DEPS)

COREPATH=..
include ../../Makefile.defs

# implicit rules
%.o : %.cpp %.d ../../Makefile.defs
	$(CXX) -c -o $@ $< $(CPPFLAGS) $(CXXFLAGS)

%.d : %.cpp %.h ../../Makefile.defs
	$(CXX) -MM $< $(CPPFLAGS) $(CXXFLAGS) > $@

%.d : ../%.cpp ../%.h ../../Makefile.defs
	$(CXX) -MM $< $(CPPFLAGS) $(CXXFLAGS) > $@

$(NAME): $(OBJS) $(CORE_OBJS) $(SIP_STACK) ../../Makefile.defs
	-@echo ""
	-@echo "making $(NAME)"
	$(LD) -o $(NAME) $(OBJS) $(CORE_OBJS) $(SIP_STACK) $(LDFLAGS) $(EXTRA_LDFLAGS)


ifeq '$(NAME)' '$(MAKECMDGOALS)'
include $(DEPS) $(CORE_DEPS)
endif


