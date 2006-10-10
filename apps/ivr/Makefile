
plug_in_name = ivr

include Makefile.defs
PY_VER = $(PYTHON_VERSION)

COREPATH ?=../../core

PYTHON_DIR = $(PYTHON_PREFIX)/include/python$(PY_VER)
PYTHON_LIBDIR = $(PYTHON_PREFIX)/lib/python$(PY_VER)

# put used Python modules from lib-dynload here, e.g. time, mysql, _cvs.so etc.
PYTHON_DYNLOAD_MODULES = $(wildcard $(PYTHON_LIBDIR)/lib-dynload/*.so) \
	$(wildcard $(PYTHON_LIBDIR)/site-packages/*.so)
PYTHON_module_cflags = -I$(PYTHON_DIR) -fno-strict-aliasing
PYTHON_module_ldflags = -Xlinker --export-dynamic \
			-L$(PYTHON_LIBDIR)/config \
			-lpython$(PY_VER) \
			#$(PYTHON_DYNLOAD_MODULES)

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

# On FreeBSD, remove the following flags: -ldl -lpthread 
module_ldflags = -ldl -lpthread -lutil -lm \
	$(PYTHON_module_ldflags) \
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

py_src = $(notdir $(wildcard py/*.py))
py_obj = $(py_src:.py=.pyc)


ifeq (all,$(MAKECMDGOALS))
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

.PHONY: install_python_files
install_python_files: $(modules-prefix)/$(modules-dir)
	echo "Installing Python object files..."
	for f in $(py_obj) ; do \
		if [ -n "py/$$f" ]; then \
			$(INSTALL-TOUCH) $(modules-prefix)/$(modules-dir)/$$f; \
			$(INSTALL-MODULES) py/$$f $(modules-prefix)/$(modules-dir)/$$f; \
		fi ; \
	done

