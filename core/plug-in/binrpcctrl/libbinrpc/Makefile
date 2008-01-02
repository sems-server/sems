

.PHONY: all clean
all clean:
	$(MAKE) -C src/ $@

.PHONY: test
test: all
	$(MAKE) -C tests/
testclean:
	$(MAKE) -C tests/ clean

.PHONY: tags
tags:
	cscope -RUb

.PHONY: distclean
distclean: clean testclean
	rm -rf cscope.*

.PHONY: install
install: all
	$(MAKE) -f Makefile.install $@

.PHONY: uninstall
uninstall:
	$(MAKE) -f Makefile.install $@
