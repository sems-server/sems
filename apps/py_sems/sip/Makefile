
COREPATH ?=../../../core

AR		?= ar
RANLIB		?= ranlib

PYTHON_VERSION ?= 2.4
PY_VER = $(PYTHON_VERSION)

SIP_FILES = $(wildcard *.sip)
lib_name = py_sems_lib.a

include Makefile.gen

.PHONY: cleaner
cleaner: clean
	rm -f *.cpp *.h *.sbf *~

Makefile.gen: configure.py $(SIP_FILES)
	python$(VER) configure.py

$(lib_name): $(OFILES)  Makefile.gen
	if [ -f $(lib_name) ]; then rm $(RMFLAGS) $(lib_name) ; fi
	$(AR) $(ARFLAGS) $(lib_name) $(OFILES)
	$(RANLIB) $(lib_name)

