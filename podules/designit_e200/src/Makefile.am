# Makefile.am for Design IT E200 Access+ podule

AUTOMAKE_OPTIONS = subdir-objects

if BUILD_PODULES

lib_LTLIBRARIES = libdesignit_e200.la
noinst_SCRIPTS =
CLEANFILES =

if OS_LINUX
CLEANFILES += $(noinst_SCRIPTS)
noinst_SCRIPTS += ../designit_e200.so
../designit_e200.so: libdesignit_e200.la
	cp .libs/libdesignit_e200.so ../designit_e200.so
endif
if OS_MACOSX
CLEANFILES += $(noinst_SCRIPTS)
noinst_SCRIPTS += ../designit_e200.dylib
../designit_e200.dylib: libdesignit_e200.la
	cp .libs/libdesignit_e200.dylib ../designit_e200.dylib
endif

amrefresh:	

libdesignit_e200_la_SOURCES = designit_e200.c ../../common/net/ne2000.c
libdesignit_e200_la_SOURCES += ../../common/net/net.c ../../common/net/net_slirp.c ../../common/net/slirp/bootp.c ../../common/net/slirp/cksum.c ../../common/net/slirp/debug.c ../../common/net/slirp/if.c ../../common/net/slirp/ip_icmp.c ../../common/net/slirp/ip_input.c ../../common/net/slirp/ip_output.c ../../common/net/slirp/mbuf.c ../../common/net/slirp/misc.c ../../common/net/slirp/queue.c ../../common/net/slirp/sbuf.c ../../common/net/slirp/slirp.c ../../common/net/slirp/socket.c ../../common/net/slirp/tcp_input.c ../../common/net/slirp/tcp_output.c ../../common/net/slirp/tcp_subr.c ../../common/net/slirp/tcp_timer.c ../../common/net/slirp/tftp.c ../../common/net/slirp/udp.c

libdesignit_e200_la_CFLAGS = -I../../../src -I../../common/net -I../../common/net/slirp
libdesignit_e200_la_LIBADD = @LIBS@

endif
