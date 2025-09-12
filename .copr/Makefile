# This is provided and used by Fedora COPR builds
# COPR provides outdir and spec variables when running make srpm

srpm:
	set -x
	# see https://docs.pagure.org/copr.copr/user_documentation.html#make-srpm
	echo "outdir=$(outdir) spec_path=$(spec)"
	pwd
	ls -al
	mkdir -p ~/rpmbuild/SOURCES
	make rpmtar
	rpmbuild -bs --define "_sourcedir $$HOME/rpmbuild/SOURCES" \
		--define "_specdir $$(pwd)" \
		--define "_builddir $$(pwd)" \
		--define "_srcrpmdir $(outdir)" \
		--define "_rpmdir $$(pwd)" \
		$(spec)

.PHONY: srpm