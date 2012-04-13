# makefile written for gnu make
CXX		= g++
SRC		= ./src
SHARED		= -shared
CPPFLAGS	= -I$(SRC) -fPIC -Wno-deprecated-declarations
DEBUG		= -g
OPTIMIZE	= -O2
GCCWARN		= -Wall #-Wstrict-prototypes
CXXFLAGS	= $(DEBUG) $(GCCWARN) $(OPTIMIZE) $(INCLUDES)
ARFLAGS		= rvs
VERSION		= 0.8

DESTDIR		=
prefix		= /usr
MKDIR		= mkdir -p
CP		= cp
LN		= ln -s

LIB		= ./libXmlRpc.a
LIBALT		= ./libxmlrpc++.a
SO		= ./libxmlrpc++.so.$(VERSION)

# Add your system-dependent network libs here. These are
# only used to build the tests (your application will need them too).
# Linux: none
# Solaris: -lsocket -lnsl
#SYSTEMLIBS	= -lsocket -lnsl
SYSTEMLIBS	=
LDLIBS		= $(LIB) $(SYSTEMLIBS)

OBJ		= $(SRC)/XmlRpcClient.o $(SRC)/XmlRpcDispatch.o \
		$(SRC)/XmlRpcServer.o $(SRC)/XmlRpcServerConnection.o \
		$(SRC)/XmlRpcServerMethod.o $(SRC)/XmlRpcSocket.o $(SRC)/XmlRpcSource.o \
		$(SRC)/XmlRpcUtil.o $(SRC)/XmlRpcValue.o

all:		$(LIB) $(SO) tests

$(LIB):		$(OBJ)
		$(AR) $(ARFLAGS) $(LIB) $(OBJ)
		cp $(LIB) $(LIBALT)

$(SO):		$(OBJ)
		$(CXX) -o $(SO) $(SHARED) $(OBJ)


tests:		$(LIB)
		cd test && $(MAKE) CXX=$(CXX) CXXFLAGS="$(CXXFLAGS)" SYSTEMLIBS="$(SYSTEMLIBS)"

doc doxygen:
		cd src && doxygen Doxyfile

distclean: clean

clean:
		rm -f $(SRC)/*.o
		rm -f $(SRC)/*~
		rm -f $(LIB) $(LIBALT) $(SO)
		rm -f build
		cd test && $(MAKE) clean

install:
		$(MKDIR) $(DESTDIR)$(prefix)/lib
		$(CP) $(SO) $(LIB) $(DESTDIR)$(prefix)/lib
		$(MKDIR) $(DESTDIR)$(prefix)/include
		$(CP) src/*.h $(DESTDIR)$(prefix)/include
		( cd $(DESTDIR)$(prefix)/lib; rm -f libxmlrpc++.so; $(LN) $(SO) libxmlrpc++.so )
		# Does not install tests right now	

# Debian package make target, in case you want to manually build a package
# from the distribution.
deb:
		dpkg-buildpackage -rfakeroot
