# Makefile.am for AKA31

AUTOMAKE_OPTIONS = subdir-objects

if BUILD_PODULES

lib_LTLIBRARIES = libaka31.la
noinst_SCRIPTS =
CLEANFILES =

if !OS_WINDOWS
CLEANFILES += $(noinst_SCRIPTS)
noinst_SCRIPTS += ../aka31.so
../aka31.so: libaka31.la
	cp .libs/libaka31.so ../aka31.so
endif

amrefresh:	

libaka31_la_SOURCES = aka31.c d71071l.c hdd_file.c scsi.c scsi_cd.c scsi_config.c scsi_hd.c wd33c93a.c ../../common/sound/sound_openal.c

if OS_WINDOWS
libaka31_la_SOURCES += ../../common/cdrom/cdrom-windows-ioctl.c 
endif
if OS_LINUX
libaka31_la_SOURCES += ../../common/cdrom/cdrom-linux-ioctl.c 
endif

libaka31_la_CFLAGS = -I../../../src -I../../common/cdrom -I../../common/sound
libaka31_la_LDFLAGS = -lopenal

endif
