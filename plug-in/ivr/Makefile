plug_in_name = ivr

SCRIPT = Python
TTS = y

#
# Python specific
#  (no need to change this if you want to use perl)
#
# PYTHON_VERSION might also be 2.2
# do a ls /usr/include/python2.3/Python.h to see if it's there
PY_VER = 2.3
# adjust to point to python include path
# can also be /usr/include/python$(PY_VER)
# look for Python.h in the specified path
# Python prefix is what you configured python with 
# if you built from source (e.g. ./configure --with-prefix=/usr/local)
# on debian it's often /usr, on SuSE /usr/local
PYTHON_PREFIX = /usr
PYTHON_DIR = $(PYTHON_PREFIX)/include/python$(PY_VER)
PYTHON_LIBDIR = $(PYTHON_PREFIX)/lib/python$(PY_VER)

# put used Python modules from lib-dynload here, e.g. time, mysql, _cvs.so etc.
PYTHON_DYNLOAD_MODULES = $(wildcard $(PYTHON_LIBDIR)/lib-dynload/*.so)
PYTHON_module_cflags = -I$(PYTHON_DIR)
PYTHON_module_ldflags = -Xlinker --export-dynamic \
			-L$(PYTHON_LIBDIR)/config \
			-lpython$(PY_VER) \
			$(PYTHON_DYNLOAD_MODULES)

ifeq ($(TTS), y)
#
#   flite text to speech
#
# uncomment the next lines if you want to have flite text-to-speech (ivr.say("Hello there"); ) 
FLITE_DIR = /usr/src/flite-1.2-release
ALT_FLITE_DIR = /usr/include/flite
IVR_TTS_module_ldflags =  -L$(FLITE_DIR)/lib -lm  -lflite_cmu_us_kal  -lflite_usenglish \
	-lflite_cmulex -lflite
IVR_TTS_module_cflags = -I$(FLITE_DIR)/include  -I$(ALT_FLITE_DIR) -DIVR_WITH_TTS
endif

LOCAL_INCLUDES = -I$(FLITE_DIR)/lang/usenglish

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

py_src = $(wildcard *.py)
py_obj = $(py_src: .py=.pyc)

.PHONY: python_files
python_files: 
	python$(PY_VER) py_comp -l -q . &&\
	cp *.pyc ../../lib

include ../Makefile.app_module
