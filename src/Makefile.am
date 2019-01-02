# Makefile.am for Arculator

AUTOMAKE_OPTIONS = subdir-objects
bin_PROGRAMS = arculator
noinst_SCRIPTS =
CLEANFILES =

if !OS_WINDOWS
CLEANFILES += $(noinst_SCRIPTS)
noinst_SCRIPTS += ../arculator
../arculator: arculator
	cp arculator ..
endif

amrefresh:	

# Special wxWidgets files
WINDRES = $(shell $(WX_CONFIG_PATH) --rescomp)
wx.res: wx.rc
	$(WINDRES) -i wx.rc --input-format=rc -o wx.res -O coff
 
wx-resources.cpp : arculator.xrc
	-wxrc -c arculator.xrc -o wx-resources.cc

# Arculator
arculator_SOURCES = 82c711.c 82c711_fdc.c arcrom.c arm.c cmos.c config.c cp15.c ddnoise.c disc.c disc_adf.c disc_apd.c disc_fdi.c disc_jfd.c disc_ssd.c \
 eterna.c fdi2raw.c fpa.c hostfs.c ics.c ide.c input_sdl2.c ioc.c keyboard.c main.c mem.c memc.c podules.c romload.c soundopenal.c st506.c \
 vidc.c video_sdl2.c wd1770.c wx-app.cc wx-config.cc wx-config_sel.cc wx-main.cc wx-resources.cc wx-sdl2.c

arculator_CFLAGS = $(subst -fpermissive,,$(shell $(WX_CONFIG_PATH) --cxxflags) $(shell sdl2-config --cflags))
arculator_CXXFLAGS = $(shell $(WX_CONFIG_PATH) --cxxflags) $(shell sdl2-config --cflags)
arculator_LDADD = @LIBS@

if OS_LINUX
arculator_SOURCES += podules-linux.c
arculator_LDADD += -ldl
endif

