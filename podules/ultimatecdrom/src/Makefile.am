# Makefile.am for Ultimate CD-ROM

AUTOMAKE_OPTIONS = subdir-objects

if BUILD_PODULES

lib_LTLIBRARIES = libultimatecdrom.la
noinst_SCRIPTS =
CLEANFILES =

if OS_LINUX
CLEANFILES += $(noinst_SCRIPTS)
noinst_SCRIPTS += ../ultimatecdrom.so
../ultimatecdrom.so: libultimatecdrom.la
	cp .libs/libultimatecdrom.so ../ultimatecdrom.so
endif
if OS_MACOSX
CLEANFILES += $(noinst_SCRIPTS)
noinst_SCRIPTS += ../ultimatecdrom.dylib
../ultimatecdrom.dylib: libultimatecdrom.la
	cp .libs/libultimatecdrom.dylib ../ultimatecdrom.dylib
endif

amrefresh:	

libultimatecdrom_la_SOURCES = mitsumi.c ultimatecdrom.c ../../common/sound/sound_openal.c

if OS_WINDOWS
libultimatecdrom_la_SOURCES += ../../common/cdrom/cdrom-windows-ioctl.c 
endif
if OS_LINUX
libultimatecdrom_la_SOURCES += ../../common/cdrom/cdrom-linux-ioctl.c 
endif
if OS_MACOSX
libultimatecdrom_la_SOURCES += ../../common/cdrom/cdrom-osx-ioctl.c 
endif

libultimatecdrom_la_CFLAGS = -I../../../src -I../../common/cdrom -I../../common/sound
libultimatecdrom_la_LIBADD = @LIBS@

endif
