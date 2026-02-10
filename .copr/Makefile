# Minimal Makefile for COPR builds
# COPR provides outdir and spec variables when running make srpm

srpm:
	set -x
	echo "outdir=$(outdir) spec_path=$(spec)"
	$(eval VERSION := $(shell cat VERSION))
	mkdir -p ~/rpmbuild/SOURCES
	git archive --format=tar.gz --prefix=sems-$(VERSION)/ HEAD > ~/rpmbuild/SOURCES/sems-$(VERSION).tar.gz
	rpmbuild -bs --define "_sourcedir $$HOME/rpmbuild/SOURCES" \
		--define "_specdir $$(pwd)" \
		--define "_builddir $$(pwd)" \
		--define "_srcrpmdir $(outdir)" \
		--define "_rpmdir $$(pwd)" \
		--define "version $(VERSION)" \
		$(spec)

.PHONY: srpm
