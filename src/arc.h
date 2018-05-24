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


void arc_set_cpu(int cpu, int memc);

/*ARM*/
uint32_t *usrregs[16],userregs[16],superregs[16],fiqregs[16],irqregs[16];
uint32_t armregs[16];
int armirq,armfiq;
#define PC ((armregs[15])&0x3FFFFFC)
int ins,output;

void resetarm();
void execarm(int cycles);
void dumpregs();
int databort;
uint32_t opcode,opcode2,opcode3;

int fpaena;

/*CP15*/
void resetcp15();
uint32_t readcp15(int reg);
void writecp15(int reg, uint32_t val);

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

void writememc(uint32_t a);
void writecam(uint32_t a);

/*Sound*/
void initsound();
void deinitsound();
void mixsound();
void pollsound();

/*VIDC*/
uint32_t vidcr[64];
int soundhz,soundper;
int offsetx,offsety;
int fullborders,noborders;
int hires;
int stereoimages[8];

void initvid();
void reinitvideo();
void writevidc(uint32_t v);
int vidcgetoverflow();
void pollline();
void setredrawall();
void clearbitmap();

/*Disc*/
char discname[4][512];
int curdrive;
int discchange[4];
uint8_t disc[4][2][80][16][1024]; /*Disc - E format (2 sides, 80 tracks, 5 sectors, 1024 bytes)*/
int fdctype;
int readflash[4];

void loaddisc(char *fn, int dosdisc, int drive);
void updatedisc(char *fn, int drive);

/*1772*/
void callback();
uint8_t read1772(unsigned a);
void write1772(unsigned addr, unsigned val);
void giveup1772();
int motoron;

/*82c711*/
void reset82c711();
void callbackfdc();
uint8_t read82c711(uint32_t addr);
void write82c711(uint32_t addr, uint32_t val);
uint8_t readfdcdma(uint32_t addr);
void writefdcdma(uint32_t addr, uint8_t val);

/*IDE*/
int idecallback;
void resetide();
uint16_t readidew();
void writeidew(uint16_t val);
uint8_t readide(uint32_t addr);
void writeide(uint32_t addr, uint8_t val);
void callbackide();

/*ST-506*/
void resetst506();
void callbackst506();
uint8_t readst506(uint32_t a);
uint32_t readst506l(uint32_t a);
void writest506(uint32_t addr, uint8_t val);
void writest506l(uint32_t addr, uint32_t val);

int soundena,oshack;
int fullscreen;

#ifdef WIN32
#define mousehack 0
#else
#define mousehack 1
#endif

/*Mouse*/
int ml,mr,mt,mb;

void resetmouse();

/*Keyboard*/
void initkeyboard();
void keycallback();
void keycallback2();
void updatekeys();
void writekeyboard(uint8_t v);
void sendkey(uint8_t v);

char exname[512];

/*Config*/
int fastdisc,dblscan;
int romset;
int stereo;

#if 0
/*ArculFS*/
void initarculfs();
void arculfs(int call);
#endif
/*CMOS / I2C*/
void cmosi2cchange(int nuclock, int nudata);
void loadcmos();
void savecmos();
void cmostick();

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
