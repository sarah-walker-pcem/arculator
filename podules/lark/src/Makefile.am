# Makefile.am for Lark

AUTOMAKE_OPTIONS = subdir-objects

if BUILD_PODULES

lib_LTLIBRARIES = liblark.la
noinst_SCRIPTS =
CLEANFILES =

if OS_LINUX
CLEANFILES += $(noinst_SCRIPTS)
noinst_SCRIPTS += ../lark.so
../lark.so: liblark.la
	cp .libs/liblark.so ../lark.so
endif
if OS_MACOSX
CLEANFILES += $(noinst_SCRIPTS)
noinst_SCRIPTS += ../lark.dylib
../lark.dylib: liblark.la
	cp .libs/liblark.dylib ../lark.dylib
endif

amrefresh:	

liblark_la_SOURCES = ../../common/uart/16550.c ad1848.c am7202a.c lark.c

if OS_WINDOWS
liblark_la_SOURCES += ../../common/midi/midi_win.c ../../common/sound/sound_wavein.c ../../common/sound/sound_openal.c
endif
if OS_LINUX
liblark_la_SOURCES += ../../common/midi/midi_alsa.c ../../common/sound/sound_alsain.c ../../common/sound/sound_openal.c
endif
if OS_MACOSX
liblark_la_SOURCES += ../../common/midi/midi_null.c ../../common/sound/sound_in_null.c ../../common/sound/sound_openal.c
endif

liblark_la_CFLAGS = -I../../../src -I../../common/midi -I../../common/sound -I../../common/uart
liblark_la_LIBADD = @LIBS@

endif
