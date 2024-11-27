plug_in_name = mod_py

PYTHON_VERSION ?= $(shell (python3 -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")' 2>/dev/null) || \
                        (python -c 'import sys; assert sys.version_info.major == 3; print(f"{sys.version_info.major}.{sys.version_info.minor}")' 2>/dev/null))

PY_VER = $(PYTHON_VERSION)
PY_EXE = python$(PY_VER)

# for local python build, use e.g.:
#PY_VER = 2.6
#PY_EXE = python2.6/bin/python

DSMPATH ?= ../..

PYTHON_DIR = $(shell $(PY_EXE) ./python_inc.py)
PYTHON_LIBDIR = $(shell $(PY_EXE) ./python_lib.py)

PYTHON_module_cflags = -I$(PYTHON_DIR) -fno-strict-aliasing

PYTHON_module_ldflags = -L$(PYTHON_LIBDIR)/config \
			-lpython$(PY_VER)

module_ldflags = $(PYTHON_module_ldflags)

module_cflags  = \
	$(PYTHON_module_cflags) \
	$(module_additional_cflags) \
	-DMOD_NAME=\"$(plug_in_name)\" -I$(DSMPATH)

py_src = $(notdir $(wildcard py/*.py))
py_obj = $(py_src:.py=.pyc)

COREPATH ?=$(DSMPATH)/../../core
lib_full_name = $(DSMPATH)/mods/lib/$(lib_name)
include $(DSMPATH)/mods/Makefile.dsm_module

ifneq ($(OS),macosx)
ifneq ($(OS),solaris)
PYTHON_module_ldflags += -Xlinker --export-dynamic
endif
endif
