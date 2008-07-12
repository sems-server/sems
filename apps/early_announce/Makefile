plug_in_name = early_announce

module_ldflags =
module_cflags  = 

# Uncomment last two lines if you want to keep audio in MySQL database.
# You must also install MySQL++ development files and libraries
# (http://www.tangentsoft.net/mysql++/).
#
# If your MySQL++ version is older than 2.2, you must insert
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

COREPATH ?=../../core
include $(COREPATH)/plug-in/Makefile.app_module
