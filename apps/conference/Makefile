plug_in_name = conference

module_ldflags =
module_cflags  =

# uncomment these lines if you want flite TTS for ZRTP SAS 
# module_ldflags +=  -L$(FLITE_DIR)/lib -lm  \
#      -lflite_cmu_us_kal  -lflite_usenglish \
#	-lflite_cmulex -lflite
# module_cflags += -I$(FLITE_DIR)/include  \
#       -I$(ALT_FLITE_DIR) -DWITH_SAS_TTS

# Uncomment last two lines if you want to keep audio in MySQL database.
# You must also install MySQL++ development files and libraries
# (http://www.tangentsoft.net/mysql++/).  If your MySQL++ version is older
# than 2.2, you must insert
#
#	const std::string & raw_string(int i) const
#	{
#		return data_[i];
#	}
#
# in /usr/include/mysql++/row.h.
#
# If your MySQL++ version is less than 3.0.0, you must add -DVERSION2
# to module_flags below.
#
#module_ldflags = -lmysqlpp
#module_cflags  = -DUSE_MYSQL -I/usr/include/mysql++ -I/usr/include/mysql

extra_install = $(plug_in_name)_audio

COREPATH ?=../../core
include $(COREPATH)/plug-in/Makefile.app_module
