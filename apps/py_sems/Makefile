
plug_in_name = py_sems
include Makefile.defs

COREPATH ?=../../core

TTS ?= y

# put used Python modules from lib-dynload here, e.g. time, mysql, _cvs.so etc.
PYTHON_DYNLOAD_MODULES = $(wildcard $(PYTHON_LIBDIR)/lib-dynload/*.so) \
	$(wildcard $(PYTHON_LIBDIR)/site-packages/*.so)
PYTHON_module_cflags = -Wno-write-strings -fno-strict-aliasing -I$(PYTHON_DIR) -Isip/
PYTHON_module_ldflags = -L$(PYTHON_LIBDIR)/config \
			-lpython$(PY_VER) 

ifeq ($(TTS), y)
#
#   flite text to speech
#
# uncomment the next lines if you want to have flite text-to-speech (ivr.say("Hello there"); ) 
FLITE_DIR = /usr/src/flite-1.2-release
ALT_FLITE_DIR = /usr/include/flite
PY_SEMS_TTS_module_ldflags =  -L$(FLITE_DIR)/lib -lm  -lflite_cmu_us_kal  -lflite_usenglish \
	-lflite_cmulex -lflite
PY_SEMS_TTS_module_cflags = -I$(FLITE_DIR)/include  -I$(ALT_FLITE_DIR) -DPY_SEMS_WITH_TTS
endif

LOCAL_INCLUDES = -I$(FLITE_DIR)/lang/usenglish

# On FreeBSD, remove the following flags: -ldl -lpthread 
module_ldflags = -ldl -lpthread -lm \
	$(PYTHON_module_ldflags) \
	$(PY_SEMS_TTS_module_ldflags)

module_cflags  = \
	$(PYTHON_module_cflags) \
	$(PY_SEMS_TTS_module_cflags) \
	$(module_additional_cflags)

module_extra_objs = sip/py_sems_lib.a
extra_target  = sip/py_sems_lib.a python_files
extra_install = install_python_files

py_src = $(notdir $(wildcard py/*.py))
py_obj = $(py_src:.py=.pyc)


ifeq (all,$(MAKECMDGOALS))
.PHONY: python_files
python_files: 
	$(PY_EXE) py_comp -l -q py &&\
	cp py/*.pyc $(COREPATH)/lib

endif

ifeq (,$(MAKECMDGOALS))
.PHONY: python_files
python_files: 
	$(PY_EXE) py_comp -l -q py &&\
	cp py/*.pyc $(COREPATH)/lib

endif

ifeq (module_package,$(MAKECMDGOALS))
python_files:
	$(PY_EXE) py_comp -l -q py

endif

ifeq (install,$(MAKECMDGOALS))
python_files:
	$(PY_EXE) py_comp -l -q py

endif

extra_clean = clean_py_sems_lib
LD=gcc

include $(COREPATH)/plug-in/Makefile.app_module

ifneq ($(OS),macosx)
ifneq ($(OS),solaris)
PYTHON_module_ldflags += -Xlinker --export-dynamic
endif
endif

.PHONY: install_python_files
install_python_files: $(DESTDIR)$(modules_prefix)/$(modules_dir)
	echo "Installing Python object files..."
	for f in $(py_obj) ; do \
		if [ -n "py/$$f" ]; then \
			$(INSTALL_TOUCH) $(DESTDIR)$(modules_prefix)/$(modules_dir)/$$f; \
			$(INSTALL_MODULES) py/$$f $(DESTDIR)$(modules_prefix)/$(modules_dir)/$$f; \
		fi ; \
	done

	echo "Installing py_sems script path..."
	$(INSTALL_MODULES) -d $(DESTDIR)$(modules_prefix)/$(py-sems-modules_dir)

.PHONY: clean_py_sems_lib
clean_py_sems_lib:
	COREPATH=../$(COREPATH) $(MAKE) -C sip/ clean
	rm -f py/*.pyc
	rm -f $(module_extra_objs)
sip/py_sems_lib.a:
	COREPATH=../$(COREPATH) $(MAKE) -C sip/ Makefile.gen
	COREPATH=../$(COREPATH) $(MAKE) -C sip/ py_sems_lib.a
