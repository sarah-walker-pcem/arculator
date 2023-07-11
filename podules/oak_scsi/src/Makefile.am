# Makefile.am for Oak SCSI

AUTOMAKE_OPTIONS = subdir-objects

if BUILD_PODULES

lib_LTLIBRARIES = liboak_scsi.la
noinst_SCRIPTS =
CLEANFILES =

if OS_WINDOWS
CLEANFILES += $(noinst_SCRIPTS)
noinst_SCRIPTS += ../oak_scsi.dll
../oak_scsi.dll: liboak_scsi.la
	cp .libs/liboak_scsi-0.dll ../oak_scsi.dll
liboak_scsi_la_LDFLAGS = -no-undefined
endif
if OS_LINUX
CLEANFILES += $(noinst_SCRIPTS)
noinst_SCRIPTS += ../oak_scsi.so
../oak_scsi.so: liboak_scsi.la
	cp .libs/liboak_scsi.so ../oak_scsi.so
endif
if OS_MACOSX
CLEANFILES += $(noinst_SCRIPTS)
noinst_SCRIPTS += ../oak_scsi.dylib
../oak_scsi.dylib: liboak_scsi.la
	cp .libs/liboak_scsi.dylib ../oak_scsi.dylib
endif

amrefresh:

liboak_scsi_la_SOURCES = oak_scsi.c ncr5380.c ../../common/scsi/hdd_file.c ../../common/scsi/scsi.c ../../common/scsi/scsi_cd.c ../../common/scsi/scsi_config.c ../../common/scsi/scsi_hd.c ../../common/sound/sound_out_sdl2.c ../../common/eeprom/93c06.c

if OS_WINDOWS
liboak_scsi_la_SOURCES += ../../common/cdrom/cdrom-windows-ioctl.c
endif
if OS_LINUX
liboak_scsi_la_SOURCES += ../../common/cdrom/cdrom-linux-ioctl.c
endif
if OS_MACOSX
liboak_scsi_la_SOURCES += ../../common/cdrom/cdrom-osx-ioctl.c
endif

liboak_scsi_la_CFLAGS = $(SDL_CFLAGS) -I../../../src -I../../common/cdrom -I../../common/eeprom -I../../common/sound -I../../common/scsi
liboak_scsi_la_LIBADD = @LIBS@

endif
