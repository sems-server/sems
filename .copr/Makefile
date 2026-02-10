# Minimal Makefile for COPR builds (CMake-based)
# COPR provides outdir and spec variables when running make srpm

srpm:
	set -x
	echo "outdir=$(outdir) spec_path=$(spec)"
	mkdir -p ~/rpmbuild/SOURCES
	mkdir -p build && cd build && cmake3 .. && make rpmtar
	rpmbuild -bs --define "_sourcedir $$HOME/rpmbuild/SOURCES" \
		--define "_specdir $$(pwd)" \
		--define "_builddir $$(pwd)" \
		--define "_srcrpmdir $(outdir)" \
		--define "_rpmdir $$(pwd)" \
		--define "version $$(cat VERSION)" \
		$(spec)

.PHONY: srpm
