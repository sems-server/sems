plug_in_name = voicemail

module_ldflags =
module_cflags  = 

# Uncomment the last two lines if you want to keep templates and audio
# in MySQL database.
#
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
# You must manually install default.template.sample to default_template
# database table.
#
#module_ldflags = -lmysqlpp
#module_cflags  = -DUSE_MYSQL -I/usr/include/mysql++ -I/usr/include/mysql

extra_install = install_email_template $(plug_in_name)_audio

vm_module:	all

.PHONY: install_email_template
install_email_template: $(cfg-prefix)/$(cfg-dir)
	mkdir -p $(cfg-prefix)/$(cfg-dir)
	$(INSTALL-TOUCH) $(cfg-prefix)/$(cfg-dir)default.template.sample
	$(INSTALL-CFG) default.template.sample $(cfg-prefix)/$(cfg-dir)
	if [ ! -f $(cfg-prefix)/$(cfg-dir)default.template ]; then \
		cp -f $(cfg-prefix)/$(cfg-dir)default.template.sample \
			$(cfg-prefix)/$(cfg-dir)default.template; \
	fi


COREPATH ?=../../core
include $(COREPATH)/plug-in/Makefile.app_module
