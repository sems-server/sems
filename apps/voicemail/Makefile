plug_in_name = voicemail

module_ldflags =
module_cflags  = 

# Uncomment the last two lines if you want to keep templates and audio
# in MySQL database.
#
# You must also install MySQL++ development files and libraries
# (http://www.tangentsoft.net/mysql++/).  Your MySQL++ version must be
# at least 2.2.
#
# You must manually install default.template.sample to default_template
# database table.
#
#module_ldflags = -lmysqlpp
#module_cflags  = -DUSE_MYSQL -I/usr/include/mysql++ -I/usr/include/mysql

extra_install = install_email_template $(plug_in_name)_audio

vm_module:	all

.PHONY: install_email_template
install_email_template: $(DESTDIR)$(app-cfg-target)
	$(INSTALL-TOUCH) $(DESTDIR)$(app-cfg-target)default.template.sample
	$(INSTALL-CFG) default.template.sample $(DESTDIR)$(app-cfg-target)
	if [ ! -f $(DESTDIR)$(app-cfg-target)default.template ]; then \
		cp -f $(DESTDIR)$(app-cfg-target)default.template.sample \
			$(DESTDIR)$(app-cfg-target)default.template; \
	fi


COREPATH ?=../../core
include $(COREPATH)/plug-in/Makefile.app_module
