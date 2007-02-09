
COREPATH ?=../../../core

PY_VER = 2.3
SIP_FILES = $(wildcard *.sip)

.PHONY: all
all: py_sems.so


.PHONY: clean
clean:
	$(MAKE) -f Makefile.gen clean; \
	rm -f *.cpp *.h *.sbf *~

Makefile.gen: configure.py $(SIP_FILES)
	python$(VER) configure.py

py_sems.so: Makefile.gen $(SIP_FILES)
	$(MAKE) -f Makefile.gen COREPATH=$(COREPATH)
