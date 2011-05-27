plug_in_name = db_reg_agent

module_ldflags = -lmysqlpp
module_cflags  = -DHAS_OFFER_ANSWER -DMOD_NAME=\"$(plug_in_name)\" -I/usr/include/mysql++ -I/usr/include/mysql

COREPATH ?=../../core
include $(COREPATH)/plug-in/Makefile.app_module
