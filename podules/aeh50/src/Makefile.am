# Makefile.am for Acorn AEH50 Ethernet II podule

AUTOMAKE_OPTIONS = subdir-objects

if BUILD_PODULES

lib_LTLIBRARIES = libaeh50.la
noinst_SCRIPTS =
CLEANFILES =

if OS_LINUX
CLEANFILES += $(noinst_SCRIPTS)
noinst_SCRIPTS += ../aeh50.so
../aeh50.so: libaeh50.la
	cp .libs/libaeh50.so ../aeh50.so
endif
if OS_MACOSX
CLEANFILES += $(noinst_SCRIPTS)
noinst_SCRIPTS += ../aeh50.dylib
../aeh50.dylib: libaeh50.la
	cp .libs/libaeh50.dylib ../aeh50.dylib
endif

amrefresh:	

libaeh50_la_SOURCES = aeh50.c ../../common/net/ne2000.c
libaeh50_la_SOURCES += ../../common/net/net.c ../../common/net/net_slirp.c ../../common/net/slirp/bootp.c ../../common/net/slirp/cksum.c ../../common/net/slirp/debug.c ../../common/net/slirp/if.c ../../common/net/slirp/ip_icmp.c ../../common/net/slirp/ip_input.c ../../common/net/slirp/ip_output.c ../../common/net/slirp/mbuf.c ../../common/net/slirp/misc.c ../../common/net/slirp/queue.c ../../common/net/slirp/sbuf.c ../../common/net/slirp/slirp.c ../../common/net/slirp/socket.c ../../common/net/slirp/tcp_input.c ../../common/net/slirp/tcp_output.c ../../common/net/slirp/tcp_subr.c ../../common/net/slirp/tcp_timer.c ../../common/net/slirp/tftp.c ../../common/net/slirp/udp.c

libaeh50_la_CFLAGS = -I../../../src -I../../common/net -I../../common/net/slirp
libaeh50_la_LIBADD = @LIBS@

endif
