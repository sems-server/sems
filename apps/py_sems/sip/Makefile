
COREPATH ?=../../../core

PYTHON_VERSION ?= 2.4
PY_VER = $(PYTHON_VERSION)

SIP_FILES = $(wildcard *.sip)

.PHONY: all
all: py_sems.so


.PHONY: clean
clean:
	$(MAKE) -f Makefile.gen clean


.PHONY: cleaner
cleaner: clean
	rm -f *.cpp *.h *.sbf *~

Makefile.gen: configure.py $(SIP_FILES)
	python$(PY_VER) configure.py

py_sems.so: Makefile.gen $(SIP_FILES)
	$(MAKE) -f Makefile.gen COREPATH=$(COREPATH)
