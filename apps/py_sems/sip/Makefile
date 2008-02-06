
COREPATH ?=../../../core

AR		?= ar
RANLIB		?= ranlib

include ../Makefile.defs

PY_VER = $(PYTHON_VERSION)

SIP_FILES = $(wildcard *.sip)
lib_name = py_sems_lib.a

ifneq 'Makefile.gen' '$(MAKECMDGOALS)'
include Makefile.gen
endif

.PHONY: cleaner
cleaner: clean
	rm -f *.cpp *.h *.sbf Makefile.gen $(lib_name) *~

Makefile.gen: Makefile configure.py $(SIP_FILES)
	python$(PY_VER) configure.py

ifneq 'Makefile.gen' '$(MAKECMDGOALS)'
$(lib_name): $(OFILES)  Makefile.gen
	if [ -f $(lib_name) ]; then rm $(RMFLAGS) $(lib_name) ; fi
	$(AR) $(ARFLAGS) $(lib_name) $(OFILES)
	$(RANLIB) $(lib_name)
endif
