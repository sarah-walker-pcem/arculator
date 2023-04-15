# Makefile.am for AKA16

AUTOMAKE_OPTIONS = subdir-objects

if BUILD_PODULES

lib_LTLIBRARIES = libaka16.la
noinst_SCRIPTS =
CLEANFILES =

if OS_WINDOWS
CLEANFILES += $(noinst_SCRIPTS)
noinst_SCRIPTS += ../aka16.dll
../aka16.dll: libaka16.la
	cp .libs/libaka16-0.dll ../aka16.dll
libaka16_la_LDFLAGS = -no-undefined
endif
if OS_LINUX
CLEANFILES += $(noinst_SCRIPTS)
noinst_SCRIPTS += ../aka16.so
../aka16.so: libaka16.la
	cp .libs/libaka16.so ../aka16.so
endif
if OS_MACOSX
CLEANFILES += $(noinst_SCRIPTS)
noinst_SCRIPTS += ../aka16.dylib
../aka16.dylib: libaka16.la
	cp .libs/libaka16.dylib ../aka16.dylib
endif

amrefresh:	

libaka16_la_SOURCES = ../../common/uart/scc2691.c aka16.c

if OS_WINDOWS
libaka16_la_SOURCES += ../../common/midi/midi_win.c
endif
if OS_LINUX
libaka16_la_SOURCES += ../../common/midi/midi_alsa.c
endif
if OS_MACOSX
libaka16_la_SOURCES += ../../common/midi/midi_null.c
endif

libaka16_la_CFLAGS = -I../../../src -I../../common/midi -I../../common/uart
libaka16_la_LIBADD = @LIBS@

if OS_WINDOWS
libaka16_la_LIBADD += -lwinmm
endif

endif
