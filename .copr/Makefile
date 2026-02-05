# Minimal Makefile for COPR builds (CMake-based)
# COPR provides outdir and spec variables when running make srpm

srpm:
	set -x
	echo "outdir=$(outdir) spec_path=$(spec)"
	mkdir -p ~/rpmbuild/SOURCES
	mkdir -p build && (cd build && cmake3 .. && make rpmtar)
	rpmbuild -bs --define "_sourcedir $$HOME/rpmbuild/SOURCES" \
		--define "_specdir $$(dirname $(spec))" \
		--define "_builddir $$(pwd)/build" \
		--define "_srcrpmdir $(outdir)" \
		--define "_rpmdir $$(pwd)/build" \
		$(spec)

.PHONY: srpm
