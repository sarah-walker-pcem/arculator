/*Arculator 0.8 by Tom Walker
  Main header file*/

#define printf rpclog

/*Misc*/
void rpclog(char *format, ...);
void error(char *format, ...);

/*ARM*/
unsigned long *usrregs[16],userregs[16],superregs[16],fiqregs[16],irqregs[16];
unsigned long armregs[16];
int armirq,armfiq;
#define PC ((armregs[15])&0x3FFFFFC)
int ins,output;

void resetarm();
void execarm(int cycles);
void dumpregs();
int databort;
unsigned long opcode,opcode2,opcode3;

int fpaena;

/*CP15*/
void resetcp15();
unsigned long readcp15(int reg);
void writecp15(int reg, unsigned long val);

/*IOC*/
struct
{
        unsigned char irqa,irqb,fiq;
        unsigned char mska,mskb,mskf;
        unsigned char ctrl;
        int timerc[4],timerl[4],timerr[4];
} ioc;

void resetioc();
unsigned char readioc(unsigned long a);
void writeioc(unsigned long addr, unsigned long val);
void updateirqs();
void updateioctimers();
void iocfiq(unsigned char v);
void iocfiqc(unsigned char v);

/*Memory*/
int modepritabler[3][6],modepritablew[3][6];
unsigned long *mempoint[0x4000];
unsigned char *mempointb[0x4000];
int memstat[0x4000];
unsigned long *ram,*rom;
unsigned char *romb;
int memmode;

void initmem(int memsize);
void resizemem(int memsize);
int loadrom();
void resetpagesize(int pagesize);

#define readmemb(a)    ((modepritabler[memmode][memstat[((a)>>12)&0x3FFF]] && !((a)>>26))?mempointb[((a)>>12)&0x3FFF][(a)&0xFFF]:readmemfb(a))
#define readmeml(a)    ((modepritabler[memmode][memstat[((a)>>12)&0x3FFF]] && !((a)>>26))?mempoint[((a)>>12)&0x3FFF][((a)&0xFFF)>>2]:readmemfl(a))
#define writememb(a,v) if ((a)==0x1824460) rpclog("Writeb %08X %02X %07X\n",a,v,PC); if (modepritablew[memmode][memstat[((a)>>12)&0x3FFF]] && !((a)>>26)) mempointb[((a)>>12)&0x3FFF][(a)&0xFFF]=(v&0xFF); else writememfb(a,v);
#define writememl(a,v) /*if ((a)==(0x1C01FC0+0x3C)) { rpclog("Writel %08X %08X %07X\n",a,v,PC); }*/ if (modepritablew[memmode][memstat[((a)>>12)&0x3FFF]] && !((a)>>26)) mempoint[((a)>>12)&0x3FFF][((a)&0xFFF)>>2]=v; else writememfl(a,v);
#define readmemff(a)    ((modepritabler[memmode][memstat[((a)>>12)&0x3FFF]])?mempoint[((a)>>12)&0x3FFF][((a)&0xFFF)>>2]:readmemf(a))

unsigned long readmemf(unsigned long a);
unsigned char readmemfb(unsigned long a);
unsigned long readmemfl(unsigned long a);
void writememfb(unsigned long a,unsigned char v);
void writememfl(unsigned long a,unsigned long v);

/*MEMC*/
unsigned long vinit,vstart,vend;
unsigned long cinit;
unsigned long sstart,ssend,ssend2,sptr;

void writememc(unsigned long a);
void writecam(unsigned long a);

/*Sound*/
void initsound();
void deinitsound();
void mixsound();
void pollsound();

/*VIDC*/
unsigned long vidcr[64];
int soundhz,soundper;
int offsetx,offsety;
int fullborders,noborders;
int hires;
int stereoimages[8];

void initvid();
void reinitvideo();
void writevidc(unsigned long v);
int vidcgetoverflow();
void pollline();
void setredrawall();
void clearbitmap();

/*Disc*/
char discname[4][512];
int curdrive;
int discchange[4];
int inreadop;
unsigned char disc[4][2][80][16][1024]; /*Disc - E format (2 sides, 80 tracks, 5 sectors, 1024 bytes)*/
int fdctype;
int readflash[4];

void loaddisc(char *fn, int dosdisc, int drive);
void updatedisc(char *fn, int drive);

/*1772*/
void callback();
unsigned char read1772(unsigned a);
void write1772(unsigned addr, unsigned val);
void discchangeint();
void discchangecint();
void giveup1772();
int motoron;

/*82c711*/
void reset82c711();
void callbackfdc();
unsigned char read82c711(unsigned long addr);
void write82c711(unsigned long addr, unsigned long val);
unsigned char readfdcdma(unsigned long addr);
void writefdcdma(unsigned long addr, unsigned char val);

/*FDI*/
void fdiinit(void (*func)(), void (*func2)(), void (*func3)());
void fdireadsector(int sector, int track, void (*func)(unsigned char dat, int last));
void fdiseek(int track);
void fdiload(char *fn, int drv, int type);
void fdinextbit();
int fdihd[2];
int readtrack;
int readidcommand;
void readidresult(unsigned char *dat, int badcrc);
int fdipos;

/*IDE*/
int idecallback;
void resetide();
unsigned short readidew();
void writeidew(unsigned short val);
unsigned char readide(unsigned short addr);
void writeide(unsigned short addr, unsigned char val);
void callbackide();

/*ST-506*/
void resetst506();
void callbackst506();
unsigned char readst506(unsigned long a);
unsigned long readst506l(unsigned long a);
void writest506(unsigned long addr, unsigned char val);
void writest506l(unsigned long addr, unsigned long val);

int soundena,mousehack,oshack;
int fullscreen;
int speed;
int arm3;

/*Mouse*/
int ml,mr,mt,mb;

void resetmouse();

/*Keyboard*/
void initkeyboard();
void keycallback();
void keycallback2();
void updatekeys();
void writekeyboard(unsigned char v);
void sendkey(unsigned char v);

char exname[512];

/*Config*/
int fastdisc,dblscan;
int romset;
int hardwareblit;
int stereo;

/*ArculFS*/
void initarculfs();
void arculfs(int call);

/*CMOS / I2C*/
void cmosi2cchange(int nuclock, int nudata);
void loadcmos();
void savecmos();
void cmostick();

int hdensity;
