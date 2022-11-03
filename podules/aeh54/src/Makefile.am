# Makefile.am for Acorn AEH54 Ethernet III podule

AUTOMAKE_OPTIONS = subdir-objects

if BUILD_PODULES

lib_LTLIBRARIES = libaeh54.la
noinst_SCRIPTS =
CLEANFILES =

if OS_LINUX
CLEANFILES += $(noinst_SCRIPTS)
noinst_SCRIPTS += ../aeh54.so
../aeh54.so: libaeh54.la
	cp .libs/libaeh54.so ../aeh54.so
endif
if OS_MACOSX
CLEANFILES += $(noinst_SCRIPTS)
noinst_SCRIPTS += ../aeh54.dylib
../aeh54.dylib: libaeh54.la
	cp .libs/libaeh54.dylib ../aeh54.dylib
endif

amrefresh:	

libaeh54_la_SOURCES = aeh54.c seeq8005.c
libaeh54_la_SOURCES += ../../common/net/net.c ../../common/net/net_slirp.c ../../common/net/slirp/bootp.c ../../common/net/slirp/cksum.c ../../common/net/slirp/debug.c ../../common/net/slirp/if.c ../../common/net/slirp/ip_icmp.c ../../common/net/slirp/ip_input.c ../../common/net/slirp/ip_output.c ../../common/net/slirp/mbuf.c ../../common/net/slirp/misc.c ../../common/net/slirp/queue.c ../../common/net/slirp/sbuf.c ../../common/net/slirp/slirp.c ../../common/net/slirp/socket.c ../../common/net/slirp/tcp_input.c ../../common/net/slirp/tcp_output.c ../../common/net/slirp/tcp_subr.c ../../common/net/slirp/tcp_timer.c ../../common/net/slirp/tftp.c ../../common/net/slirp/udp.c

libaeh54_la_CFLAGS = -I../../../src -I../../common/net -I../../common/net/slirp
libaeh54_la_LIBADD = @LIBS@

endif
