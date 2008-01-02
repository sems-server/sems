COREPATH =../..
plug_in_name = binrpcctrl


# Use following definitions if you whish
# to use libbinrpc included in SEMS' repos
# Remember to fetch the lib before compiling
# using: 'make libbinrpc'
#
LIBBINRPC_INC?=libbinrpc/include
LIBBINRPC_LIB?=libbinrpc/src
module_ldflags = $(LIBBINRPC_LIB)/libbinrpc.a

# Use following definitions if you installed libbinrpc
#
#LIBBINRPC_INC?=/usr/local/include
#LIBBINRPC_LIB?=/usr/local/lib
#module_ldflags = -L$(LIBBINRPC_LIB) -lbinrpc

module_cflags  = -DMOD_NAME=\"$(plug_in_name)\" -I$(LIBBINRPC_INC)

%.o : %.c %.d
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

%.d : %.c %.h Makefile
	$(CC) -MM $< $(CFLAGS) $(CPPFLAGS) > $@

libbinrpc:
	svn co http://svn.berlios.de/svnroot/repos/libbinrpc/trunk/ && \
	mv trunk libbinrpc && \
	cd libbinrpc && \
	$(MAKE) all

include ../Makefile.app_module 
