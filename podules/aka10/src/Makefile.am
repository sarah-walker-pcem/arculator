# Makefile.am for AKA10

AUTOMAKE_OPTIONS = subdir-objects

if BUILD_PODULES

lib_LTLIBRARIES = libaka10.la
noinst_SCRIPTS =
CLEANFILES =

if OS_LINUX
CLEANFILES += $(noinst_SCRIPTS)
noinst_SCRIPTS += ../aka10.so
../aka10.so: libaka10.la
	cp .libs/libaka10.so ../aka10.so
endif
if OS_MACOSX
CLEANFILES += $(noinst_SCRIPTS)
noinst_SCRIPTS += ../aka10.dylib
../aka10.dylib: libaka10.la
	cp .libs/libaka10.dylib ../aka10.dylib
endif

amrefresh:	

libaka10_la_SOURCES = ../../common/uart/6850.c aka10.c

if OS_WINDOWS
libaka10_la_SOURCES += ../../common/midi/midi_win.c
endif
if OS_LINUX
libaka10_la_SOURCES += ../../common/midi/midi_alsa.c
endif
if OS_MACOSX
libaka10_la_SOURCES += ../../common/midi/midi_null.c
endif

libaka10_la_CFLAGS = -I../../../src -I../../common/midi -I../../common/uart
libaka10_la_LIBADD = @LIBS@

endif
