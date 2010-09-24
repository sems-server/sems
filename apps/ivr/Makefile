plug_in_name = ivr

include Makefile.defs

COREPATH ?=../../core

PYTHON_DIR = $(shell $(PY_EXE) ./python_inc.py)
PYTHON_LIBDIR = $(shell $(PY_EXE) ./python_lib.py)

# put used Python modules from lib-dynload here, e.g. time, mysql, _cvs.so etc.
PYTHON_module_cflags = -I$(PYTHON_DIR) -fno-strict-aliasing -Wno-write-strings

PYTHON_module_ldflags = -L$(PYTHON_LIBDIR)/config \
			-lpython$(PY_VER)

#PYTHON_module_ldflags = -Xlinker --export-dynamic \
#			-L$(PYTHON_LIBDIR)/config \
#			-lpython$(PY_VER)



ifeq ($(TTS), y)
#
#   flite text to speech
#
IVR_TTS_module_ldflags =  -L$(FLITE_DIR)/lib -lm  -lflite_cmu_us_kal  -lflite_usenglish \
	-lflite_cmulex -lflite
IVR_TTS_module_cflags = -I$(FLITE_DIR)/include  -I$(ALT_FLITE_DIR) -DIVR_WITH_TTS
else
FLITE_DIR = 
ALT_FLITE_DIR =
endif

LOCAL_INCLUDES = -I$(FLITE_DIR)/lang/usenglish

module_ldflags = $(PYTHON_module_ldflags) \
	$(IVR_TTS_module_ldflags)

# for perl support:
# -DIVR_PERL  `perl -MExtUtils::Embed -e ccopts `
# for flite text-to-speech support -DIVR_WITH_TTS
module_cflags  = \
	$(PYTHON_module_cflags) \
	$(IVR_TTS_module_cflags) \
	$(PERL_module_cflags) \
	$(module_additional_cflags)

extra_target  = python_files
extra_install = install_python_files
extra_clean = clean_python_files

py_src = $(notdir $(wildcard py/*.py))
py_obj = $(py_src:.py=.pyc)

ifeq (,$(MAKECMDGOALS))
.PHONY: python_files
python_files: 
	$(PY_EXE) py_comp -l -q py &&\
	cp py/*.pyc $(COREPATH)/lib

endif

ifeq (all,$(MAKECMDGOALS))
.PHONY: python_files
python_files: 
	$(PY_EXE) py_comp -l -q py &&\
	cp py/*.pyc $(COREPATH)/lib

endif

ifeq (install,$(MAKECMDGOALS))
.PHONY: python_files
python_files: 
	python$(PY_VER) py_comp -l -q py &&\
	cp py/*.pyc $(COREPATH)/lib

endif

ifeq (module_package,$(MAKECMDGOALS))
python_files:
	python$(PY_VER) py_comp -l -q py

endif

include $(COREPATH)/plug-in/Makefile.app_module

ifneq ($(OS),macosx)
ifneq ($(OS),solaris)
PYTHON_module_ldflags += -Xlinker --export-dynamic
endif
endif

.PHONY: install_python_files
install_python_files: $(DESTDIR)$(modules-prefix)/$(modules-dir)
	echo "Installing Python object files..."
	for f in $(py_obj) ; do \
		if [ -n "py/$$f" ]; then \
			$(INSTALL-TOUCH) $(DESTDIR)$(modules-prefix)/$(modules-dir)/$$f; \
			$(INSTALL-MODULES) py/$$f $(DESTDIR)$(modules-prefix)/$(modules-dir)/$$f; \
		fi ; \
	done

.PHONY: clean_python_files
clean_python_files:
	rm -f py/*.pyc
	rm -f $(COREPATH)/lib/*.pyc
