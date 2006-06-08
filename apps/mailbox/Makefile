
NAME=mailbox
VERSION=2.2.0-1

PYTHON_VERSION=2.3

LIBDIR=imap_mailbox
LIB_INSTALLDIR="/usr/lib/sems/ivr"
TARBALL_PREFIX=sems-app-${NAME}
TARBALL="${TARBALL_PREFIX}-${VERSION}.tar.gz"

BASEDIR?=${basedir}

GROUP="root"
OWNER="root"

BIN_PERMISSIONS="755"
LIB_PERMISSIONS="644"

PYCHECKERARGS = --stdlib
PYCHECKERDOCARGS = --classdoc --funcdoc

.PHONY: all
all: clean compile

clean:
	find . -iname "*\.pyc" -o -iname "*\.py~" | xargs rm -f
	rm -f ${TARBALL_PREFIX}*.tar.gz

compile: 
	python${PYTHON_VERSION} py_comp -q .

install: all
	install -d ${BASEDIR}/${LIB_INSTALLDIR}
	install -m ${LIB_PERMISSIONS} -o ${OWNER} -g ${GROUP} *.pyc ${BASEDIR}/${LIB_INSTALLDIR}
	install -d ${BASEDIR}/${LIB_INSTALLDIR}/${LIBDIR}
	install -m ${LIB_PERMISSIONS} -o ${OWNER} -g ${GROUP} ${LIBDIR}/*.pyc ${BASEDIR}/${LIB_INSTALLDIR}/${LIBDIR}

uninstall: 
	@echo "please remove the files from ${LIB_INSTALLDIR} manually."

fulltest: 
	find | grep /Test | grep -v ".svn" | grep \\.py$$ | sed -e "s#^./##g" | bash -e - 

check: 
	find ${LIBDIR}/ | grep \\.py$$ | grep -v Test | PYTHONPATH=$(PYTHONPATH):../ivr/moc xargs pychecker ${PYCHECKERARGS} 

doccheck: 
	find ${LIBDIR}/  | grep \\.py$$ | grep -v Test | xargs pychecker ${PYCHECKERARGS} ${PYCHECKERDOCARGS}

dist: all
	tar -cvzf ${TARBALL} . --exclude=*.tar.gz \
				--exclude=.svn \
				--exclude=*~ \
				--exclude=*.pyc



