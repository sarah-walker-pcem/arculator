# Makefile.am for Lark

AUTOMAKE_OPTIONS = subdir-objects

if BUILD_PODULES

lib_LTLIBRARIES = liblark.la
noinst_SCRIPTS =
CLEANFILES =

if !OS_WINDOWS
CLEANFILES += $(noinst_SCRIPTS)
noinst_SCRIPTS += ../lark.so
../lark.so: liblark.la
	cp .libs/liblark.so ../lark.so
endif

amrefresh:	

liblark_la_SOURCES = ../../common/uart/16550.c ad1848.c am7202a.c lark.c ../../common/midi/midi_alsa.c ../../common/sound/sound_alsain.c ../../common/sound/sound_openal.c

liblark_la_CFLAGS = -I../../../src -I../../common/midi -I../../common/sound -I../../common/uart

endif
