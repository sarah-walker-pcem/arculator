# Makefile.am for Aleph One 386/486PC Expansion Card

AUTOMAKE_OPTIONS = subdir-objects

if BUILD_PODULES

lib_LTLIBRARIES = libpccard.la
noinst_SCRIPTS =
CLEANFILES =

if OS_WINDOWS
CLEANFILES += $(noinst_SCRIPTS)
noinst_SCRIPTS += ../pccard.dll
../pccard.dll: libpccard.la
	cp .libs/libpccard-0.dll ../pccard.dll
libpccard_la_LDFLAGS = -no-undefined
endif
if OS_LINUX
CLEANFILES += $(noinst_SCRIPTS)
noinst_SCRIPTS += ../pccard.so
../pccard.so: libpccard.la
	cp .libs/libpccard.so ../pccard.so
endif
if OS_MACOSX
CLEANFILES += $(noinst_SCRIPTS)
noinst_SCRIPTS += ../pccard.dylib
../pccard.dylib: libpccard.la
	cp .libs/libpccard.dylib ../pccard.dylib
endif

amrefresh:	

libpccard_la_SOURCES = pccard_podule.c
libpccard_la_SOURCES += pcem/808x.c pcem/386.c pcem/386_common.c pcem/386_dynarec.c pcem/cpu.c pcem/cpu_tables.c pcem/diva.c pcem/dma.c pcem/io.c pcem/lpt.c pcem/mem.c pcem/pc.c pcem/pic.c pcem/pit.c pcem/scamp.c pcem/serial.c pcem/timer.c pcem/x86seg.c pcem/x87.c pcem/x87_timings.c
libpccard_la_SOURCES += libco/libco.c

libpccard_la_CFLAGS = -I../../../src
libpccard_la_LIBADD = @LIBS@

endif
