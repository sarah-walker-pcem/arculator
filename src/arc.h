/*Arculator 0.8 by Tom Walker
  Main header file*/

//#define printf rpclog

#include <stdio.h>
#include <stdint.h>

/*Misc*/
extern void rpclog(const char *format, ...);
extern void error(const char *format, ...);
extern void fatal(const char *format, ...);

#ifdef DEBUG_CMOS
#define LOG_CMOS rpclog
#else
#define LOG_CMOS(...) ((void)0)
#endif

#ifdef DEBUG_EVENT_LOOP
#define LOG_EVENT_LOOP rpclog
#else
#define LOG_EVENT_LOOP(...) ((void)0)
#endif

#ifdef DEBUG_KB_MOUSE
#define LOG_KB_MOUSE rpclog
#else
#define LOG_KB_MOUSE(...) ((void)0)
#endif

#ifdef DEBUG_MEMC_VIDEO
#define LOG_MEMC_VIDEO rpclog
#else
#define LOG_MEMC_VIDEO(...) ((void)0)
#endif

#ifdef DEBUG_VIDC_REGISTERS
#define LOG_VIDC_REGISTERS rpclog
#else
#define LOG_VIDC_REGISTERS(...) ((void)0)
#endif

#ifdef DEBUG_VIDC_TIMING
#define LOG_VIDC_TIMING rpclog
#else
#define LOG_VIDC_TIMING(...) ((void)0)
#endif

#ifdef DEBUG_VIDEO_FRAMES
#define LOG_VIDEO_FRAMES rpclog
#else
#define LOG_VIDEO_FRAMES(...) ((void)0)
#endif

#ifdef DEBUG_DATABORT
#define LOG_DATABORT(...) do { rpclog(__VA_ARGS__); dumpregs(); } while (0)
#else
#define LOG_DATABORT(...) ((void)0)
#endif


extern void arc_set_cpu(int cpu, int memc);
extern void arc_setspeed(int mhz);
extern void updatewindowsize(int x, int y);

/*ARM*/
uint32_t *usrregs[16],userregs[16],superregs[16],fiqregs[16],irqregs[16];
uint32_t armregs[16];
int armirq,armfiq;
extern int cycles;
#define PC ((armregs[15])&0x3FFFFFC)
int ins,output;

int databort;
uint32_t opcode,opcode2,opcode3;
extern void resetarm();
extern void execarm(int cycs);
extern void dumpregs();

int fpaena;

/*CP15*/
extern void resetcp15();
extern uint32_t readcp15(int reg);
extern void writecp15(int reg, uint32_t val);

/*IOC*/
struct
{
        uint8_t irqa,irqb,fiq;
        uint8_t mska,mskb,mskf;
        uint8_t ctrl;
        int timerc[4],timerl[4],timerr[4];
} ioc;

/*Memory*/
extern int modepritabler[3][6],modepritablew[3][6];
extern uint32_t *mempoint[0x4000];
extern uint8_t *mempointb[0x4000];
extern int memstat[0x4000];
extern uint32_t *ram,*rom;
extern uint8_t *romb;
extern int memmode;

extern void initmem(int memsize);
extern void resizemem(int memsize);
extern int loadrom();
extern void resetpagesize(int pagesize);

#define readmemb(a)    ((modepritabler[memmode][memstat[((a)>>12)&0x3FFF]] && !((a)>>26))?mempointb[((a)>>12)&0x3FFF][(a)&0xFFF]:readmemfb(a))
#define readmeml(a)    ((modepritabler[memmode][memstat[((a)>>12)&0x3FFF]] && !((a)>>26))?mempoint[((a)>>12)&0x3FFF][((a)&0xFFF)>>2]:readmemfl(a))
#define writememb(a,v) do { if (modepritablew[memmode][memstat[((a)>>12)&0x3FFF]] && !((a)>>26)) mempointb[((a)>>12)&0x3FFF][(a)&0xFFF]=(v&0xFF); else { writememfb(a,v); } } while (0)
#define writememl(a,v) do { if (modepritablew[memmode][memstat[((a)>>12)&0x3FFF]] && !((a)>>26)) mempoint[((a)>>12)&0x3FFF][((a)&0xFFF)>>2]=v; else { writememfl(a,v); } } while (0)
#define readmemff(a)    ((modepritabler[memmode][memstat[((a)>>12)&0x3FFF]])?mempoint[((a)>>12)&0x3FFF][((a)&0xFFF)>>2]:readmemf(a))

extern uint32_t readmemf(uint32_t a);
extern uint8_t readmemfb(uint32_t a);
extern uint32_t readmemfl(uint32_t a);
extern void writememfb(uint32_t a,uint8_t v);
extern void writememfl(uint32_t a,uint32_t v);

/*arcrom*/
extern void resetarcrom();
extern void writearcrom(uint32_t addr, uint8_t val);
extern uint8_t readarcrom(uint32_t addr);

/*ics (idefs, icsrom)*/
extern void resetics();
extern void writeics(uint32_t addr, uint8_t val);
extern uint8_t readics(uint32_t addr);

/*MEMC*/
uint32_t vinit,vstart,vend;
uint32_t cinit;
uint32_t sstart,ssend,ssend2,sptr;

extern void initmemc();
extern void writememc(uint32_t a);
extern void writecam(uint32_t a);

/*Sound*/
extern void initsound();
extern void deinitsound();
extern void mixsound();
extern void pollsound();

/*VIDC*/
extern int soundper;
extern int offsetx,offsety;
extern int fullscreen;
extern int fullborders,noborders;
extern int hires;
extern int stereoimages[8];
extern int flyback;

extern void initvid();
extern void reinitvideo();
extern void writevidc(uint32_t v);
extern int vidcgetoverflow();
extern void pollline();
extern void setredrawall();
extern void clearbitmap();

/*Disc*/
extern char discname[4][512];
extern int curdrive;
extern int discchange[4];
extern uint8_t disc[4][2][80][16][1024]; /*Disc - E format (2 sides, 80 tracks, 5 sectors, 1024 bytes)*/
extern int fdctype;
extern int readflash[4];
extern int fastdisc;

extern void loaddisc(char *fn, int dosdisc, int drive);
extern void updatedisc(char *fn, int drive);

/*1772*/
int motoron;
extern void callback();
extern uint8_t read1772(unsigned a);
extern void write1772(unsigned addr, unsigned val);
extern void giveup1772();

/*82c711*/
extern void reset82c711();
extern void callbackfdc();
extern uint8_t read82c711(uint32_t addr);
extern void write82c711(uint32_t addr, uint32_t val);
extern uint8_t readfdcdma(uint32_t addr);
extern void writefdcdma(uint32_t addr, uint8_t val);

/*IDE*/
int idecallback;
extern void resetide();
extern uint16_t readidew();
extern void writeidew(uint16_t val);
extern uint8_t readide(uint32_t addr);
extern void writeide(uint32_t addr, uint8_t val);
extern void callbackide();

/*ST-506*/
extern void resetst506();
extern void callbackst506();
extern uint8_t readst506(uint32_t a);
extern uint32_t readst506l(uint32_t a);
extern void writest506(uint32_t addr, uint8_t val);
extern void writest506l(uint32_t addr, uint32_t val);

int soundena,oshack;
int fullscreen;

/*Causes a databort during RISC OS 3.11 startup*/
#define mousehack 0

/*Mouse*/
int ml,mr,mt,mb;

extern void doosmouse();
extern void setmousepos(uint32_t a);
extern void getunbufmouse(uint32_t a);
extern void getosmouse();
extern void setmouseparams(uint32_t a);
extern void resetmouse();

/*Keyboard*/

char exname[512];
extern void initkeyboard();
extern void keycallback();
extern void keycallback2();
extern void updatekeys();
extern void writekeyboard(uint8_t v);
extern void sendkey(uint8_t v);
extern void keyboard_poll();
extern uint8_t keyboard_read();
extern void keyboard_write(uint8_t val);

/*Config*/
int fastdisc,dblscan;
int romset;
int stereo;

#if 0
/*ArculFS*/
extern void initarculfs();
extern void arculfs(int call);
#endif

/*CMOS / I2C*/
extern int i2cclock,i2cdata;

extern void cmosi2cchange(int nuclock, int nudata);
extern void loadcmos();
extern void savecmos();
extern void cmostick();

int hdensity;



extern int limitspeed;

extern int soundena,stereo;

extern int hires;
extern int fullborders,noborders;
extern int firstfull;
extern int dblscan;

extern int arm3;
extern int fpaena;

extern int memsize;

extern int fastdisc;
extern int fdctype;


extern int speed_mhz;
extern int speed_to_mhz[5];


extern int mousecapture;


void updateins();

void get_executable_name(char *s, int size);
void arc_setspeed(int mhz);


void updatewindowsize(int x, int y);

void arc_init();
void arc_reset();
void arc_run();
void arc_close();
