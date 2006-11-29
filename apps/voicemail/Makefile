plug_in_name = voicemail

module_ldflags =
module_cflags  = 

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
