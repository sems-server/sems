NAME=sems_tests

SIP_STACK_DIR=../sip
SIP_STACK=$(SIP_STACK_DIR)/sip_stack.a
RESAMPLE_DIR=../resample
LIBRESAMPLE=$(RESAMPLE_DIR)/libresample.a
CORE_SRCS=$(filter-out ../sems.cpp , $(wildcard ../*.cpp))
CORE_HDRS=$(CORE_SRCS:.cpp=.h)
CORE_OBJS=$(CORE_SRCS:.cpp=.o)
CORE_DEPS=$(subst ../,,$(CORE_SRCS:.cpp=.d))

SBC_DIR=../../apps/sbc/
SBC_SRCS=$(wildcard $(SBC_DIR)*.cpp)
SBC_HDRS=$(SBC_SRCS:.cpp=.h)
SBC_OBJS=$(SBC_SRCS:.cpp=.o)
SBC_DEPS=$(subst $(SBC_DIR),,$(SBC_SRCS:.cpp=.d))

AUTH_DIR=../plug-in/uac_auth
AUTH_OBJS=$(AUTH_DIR)/UACAuth.o

SRCS=$(wildcard *.cpp)
HDRS=$(SRCS:.cpp=.h)
OBJS=$(SRCS:.cpp=.o)
DEPS=$(SRCS:.cpp=.d)

CPPFLAGS += -I.. -DNOMAIN

EXTRA_LDFLAGS += -lresolv -levent -levent_pthreads

.PHONY: all
all: ../../Makefile.defs sip_stack libresample
	-@$(MAKE) core_deps   && $(MAKE) sbc_deps  && $(MAKE) deps    && \
	  $(MAKE) $(NAME) && \
	./$(NAME)

.PHONY: sip_stack
sip_stack:
	-@echo ""
	-@echo "making SIP stack"
	-@cd $(SIP_STACK_DIR); $(MAKE) all

.PHONY: libresample
libresample:
	-@echo ""
	-@echo "making libresample"
	-@cd $(RESAMPLE_DIR); $(MAKE) all

SIP_STACK: sip_stack

.PHONY: clean
clean:
	rm -f $(OBJS) $(DEPS) $(CORE_DEPS) $(CORE_OBJS) $(NAME)

.PHONY: deps
deps: $(DEPS)

.PHONY: core_deps
core_deps: $(CORE_DEPS)

.PHONY: sbc_deps
sbc_deps: $(SBC_DEPS)

AUTH_OBJS: $(AUTH_DIR)/UACAuth.cpp $(AUTH_DIR)/UACAuth.h
	cd $(AUTH_DIR) ; $(MAKE) AUTH_OBJS

COREPATH=..
include ../../Makefile.defs

# implicit rules
%.o : %.cpp %.d ../../Makefile.defs
	$(CXX) -c -o $@ $< $(CPPFLAGS) $(CXXFLAGS)

%.d : %.cpp %.h ../../Makefile.defs
	$(CXX) -MM $< $(CPPFLAGS) $(CXXFLAGS) > $@

%.d : $(SBC_DIR)%.cpp $(SBC_DIR)%.h ../../Makefile.defs
	$(CXX) -MM $< $(CPPFLAGS) $(CXXFLAGS) > $@

%.d : ../%.cpp ../%.h ../../Makefile.defs
	$(CXX) -MM $< $(CPPFLAGS) $(CXXFLAGS) > $@

$(NAME): $(OBJS) $(CORE_OBJS) $(SBC_OBJS) $(SIP_STACK) $(LIBRESAMPLE) ../../Makefile.defs
	-@echo ""
	-@echo "making $(NAME)"
	$(LD) -o $(NAME) $(OBJS) $(CORE_OBJS) $(SBC_OBJS) $(SIP_STACK) $(LIBRESAMPLE) $(LDFLAGS) $(EXTRA_LDFLAGS) $(AUTH_OBJS)

ifeq '$(NAME)' '$(MAKECMDGOALS)'
include $(DEPS) $(CORE_DEPS) $(SBC_DEPS)
endif


