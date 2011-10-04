plug_in_name = db_reg_agent

MYSQLPP_DEFS = -DMYSQLPP_MYSQL_HEADERS_BURIED

module_ldflags = -lmysqlpp
module_cflags  = -DHAS_OFFER_ANSWER -DMOD_NAME=\"$(plug_in_name)\" $(MYSQLPP_DEFS)

COREPATH ?=../../core
include $(COREPATH)/plug-in/Makefile.app_module
