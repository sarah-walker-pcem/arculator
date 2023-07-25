# Makefile.am for Morley A3000 User and Analogue Port

AUTOMAKE_OPTIONS = subdir-objects

if BUILD_PODULES

lib_LTLIBRARIES = libmorley_uap.la
noinst_SCRIPTS =
CLEANFILES =

if OS_WINDOWS
CLEANFILES += $(noinst_SCRIPTS)
noinst_SCRIPTS += ../morley_uap.dll
../morley_uap.dll: libmorley_uap.la
	cp .libs/libmorley_uap-0.dll ../morley_uap.dll
libmorley_uap_la_LDFLAGS = -no-undefined
endif
if OS_LINUX
CLEANFILES += $(noinst_SCRIPTS)
noinst_SCRIPTS += ../morley_uap.so
../morley_uap.so: libmorley_uap.la
	cp .libs/libmorley_uap.so ../morley_uap.so
endif
if OS_MACOSX
CLEANFILES += $(noinst_SCRIPTS)
noinst_SCRIPTS += ../morley_uap.dylib
../morley_uap.dylib: libmorley_uap.la
	cp .libs/libmorley_uap.dylib ../morley_uap.dylib
endif

amrefresh:	

libmorley_uap_la_SOURCES = morley_uap.c ../../common/joystick/joystick_sdl2.c ../../common/adc/d7002c.c ../../common/misc/6522.c

libmorley_uap_la_CFLAGS = -I../../../src -I../../common/adc -I../../common/joystick -I../../common/misc $(SDL_CFLAGS)
libmorley_uap_la_LIBADD = @LIBS@

endif
