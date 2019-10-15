# Makefile.am for MIDI Max

AUTOMAKE_OPTIONS = subdir-objects

if BUILD_PODULES

lib_LTLIBRARIES = libmidimax.la
noinst_SCRIPTS =
CLEANFILES =

if !OS_WINDOWS
CLEANFILES += $(noinst_SCRIPTS)
noinst_SCRIPTS += ../midimax.so
../midimax.so: libmidimax.la
	cp .libs/libmidimax.so ../midimax.so
endif

amrefresh:	

# MIDI Max
libmidimax_la_SOURCES = ../../common/uart/16550.c ../../common/midi/midi_alsa.c midimax.c

libmidimax_la_CFLAGS = -I../../../src -I../../common/midi -I../../common/uart

endif
