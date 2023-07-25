# Makefile.am for AKA10

AUTOMAKE_OPTIONS = subdir-objects

if BUILD_PODULES

lib_LTLIBRARIES = libaka10.la
noinst_SCRIPTS =
CLEANFILES =

if OS_WINDOWS
CLEANFILES += $(noinst_SCRIPTS)
noinst_SCRIPTS += ../aka10.dll
../aka10.dll: libaka10.la
	cp .libs/libaka10-0.dll ../aka10.dll
libaka10_la_LDFLAGS = -no-undefined
endif
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

libaka10_la_SOURCES = ../../common/uart/6850.c aka10.c ../../common/joystick/joystick_sdl2.c ../../common/adc/d7002c.c ../../common/misc/6522.c

if OS_WINDOWS
libaka10_la_SOURCES += ../../common/midi/midi_win.c
endif
if OS_LINUX
libaka10_la_SOURCES += ../../common/midi/midi_alsa.c
endif
if OS_MACOSX
libaka10_la_SOURCES += ../../common/midi/midi_null.c
endif

libaka10_la_CFLAGS = -I../../../src -I../../common/adc -I../../common/joystick -I../../common/midi -I../../common/misc -I../../common/uart $(SDL_CFLAGS)
libaka10_la_LIBADD = @LIBS@

if OS_WINDOWS
libaka10_la_LIBADD += -lwinmm
endif

endif
