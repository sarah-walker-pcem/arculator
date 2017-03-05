VPATH = . resid-fp
CPP  = g++
CC   = gcc
WINDRES = windres.exe
CFLAGS = -O3 -ffast-math -fomit-frame-pointer -falign-loops -falign-jumps -falign-functions -DLINUX -ggdb
OBJ = 82c711.o 82c711_fdc.o arcrom.o arm.o cmos.o config.o cp15.o ddnoise.o disc.o disc_adf.o disc_apd.o disc_fdi.o disc_jfd.o disc_ssd.o eterna.o fdi2raw.o fpa.o hostfs.o ics.o ide.o ioc.o keyboard.o main.o mem.o memc.o podules.o podules-linux.o romload.o soundopenal.o st506.o vidc.o wd1770.o win.o

LIBS = -lalleg -lz -lalut -lopenal -lstdc++ -lm -ldl

Arculator.exe: $(OBJ) $(SIDOBJ)
	$(CC) $(OBJ) -o Arculator $(LIBS)

all : Arculator.exe

clean :
	rm *.o

%.o : %.c
	$(CC) $(CFLAGS) -c $<

%.o : %.cc
	$(CPP) $(CFLAGS) -c $<
