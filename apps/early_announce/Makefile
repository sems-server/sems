plug_in_name = early_announce

module_ldflags =
module_cflags  = 

# Uncomment the next two lines if you want to keep audio in MySQL database.
module_ldflags = -lmysqlcppconn
module_cflags  = -DUSE_MYSQL -I/usr/include/mysql

COREPATH ?=../../core
include $(COREPATH)/plug-in/Makefile.app_module
