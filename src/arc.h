/*Arculator 0.8 by Tom Walker
  Main header file*/

//#define printf rpclog

#include <stdint.h>

/*Misc*/
void rpclog(char *format, ...);
void error(char *format, ...);

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
uint32_t *mempoint[0x4000];
uint8_t *mempointb[0x4000];
int memstat[0x4000];
uint32_t *ram,*rom;
uint8_t *romb;
int memmode;

void initmem(int memsize);
void resizemem(int memsize);
int loadrom();
void resetpagesize(int pagesize);

#define readmemb(a)    ((modepritabler[memmode][memstat[((a)>>12)&0x3FFF]] && !((a)>>26))?mempointb[((a)>>12)&0x3FFF][(a)&0xFFF]:readmemfb(a))
#define readmeml(a)    ((modepritabler[memmode][memstat[((a)>>12)&0x3FFF]] && !((a)>>26))?mempoint[((a)>>12)&0x3FFF][((a)&0xFFF)>>2]:readmemfl(a))
#define writememb(a,v) if (modepritablew[memmode][memstat[((a)>>12)&0x3FFF]] && !((a)>>26)) mempointb[((a)>>12)&0x3FFF][(a)&0xFFF]=(v&0xFF); else writememfb(a,v);
#define writememl(a,v) if (modepritablew[memmode][memstat[((a)>>12)&0x3FFF]] && !((a)>>26)) mempoint[((a)>>12)&0x3FFF][((a)&0xFFF)>>2]=v; else writememfl(a,v);
#define readmemff(a)    ((modepritabler[memmode][memstat[((a)>>12)&0x3FFF]])?mempoint[((a)>>12)&0x3FFF][((a)&0xFFF)>>2]:readmemf(a))

uint32_t readmemf(uint32_t a);
uint8_t readmemfb(uint32_t a);
uint32_t readmemfl(uint32_t a);
void writememfb(uint32_t a,uint8_t v);
void writememfl(uint32_t a,uint32_t v);

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

/*FDI*/
void fdiinit(void (*func)(), void (*func2)(), void (*func3)());
void fdireadsector(int sector, int track, void (*func)(uint8_t dat, int last));
void fdiseek(int track);
void fdiload(char *fn, int drv, int type);
void fdinextbit();
int fdihd[2];
int readtrack;
int readidcommand;
void readidresult(uint8_t *dat, int badcrc);
int fdipos;

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
int hardwareblit;
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
extern int hardwareblit;

extern int arm3;
extern int fpaena;

extern int memsize;

extern int fastdisc;
extern int fdctype;


extern int speed_mhz;
extern int speed_to_mhz[5];


extern int mousecapture;
