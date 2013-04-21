/*NOTE TO SELF - REWRITE LDM/STM*/

/*Pandora's Box -
  Reset routine at &1800FEC
  Jumps to reset routine from &1800EA4
  Calls ADFS_DiscOp with R1=1 (read), disc address &C7C00
   buffer in module space, length 1024 bytes
   Expects it to fail, but on Arculator it passes
   possible failures - ID CRC, Data CRC, Sector not found,
   none of which seem to be true
  Protection check is in utility TheEarths, which decrypts & loads
  a module from it's own memory into RMA*/

/*Arculator 0.8 by Tom Walker
  ARM2 & ARM3 emulation*/
#include <stdio.h>
#include "arc.h"
#include <allegro.h>
#include <winalleg.h>

int fpaena=0;
int swioutput=0;
#define writememfast(a,v) writememl(a,v)
unsigned long oldr3,oldr1;
int inssinceswi=0;
unsigned long lastswi;
int keyscount=100;
int idecallback;
int fdci=200;
int motoron;
int fdccallback;
int fdicount=16;
int irq;
int cycles;
int prefabort;
int disciint=400000,discrint;
unsigned long oldr2,oldr1,opc,oopc;
void refillpipeline();
void refillpipeline2();
char bigs[256];
int bigcyc;
int times=0;
FILE *olog,*alog;
unsigned char flaglookup[16][16];
unsigned long rotatelookup[4096];
int dischack,fastvsync,vsyncbreak;
int disccint=0;
int pc38dd=0;
unsigned long oldpc,oldoldpc,oldr11;
int timetolive=0;
int inscount;
int soundtime;
int discint;
unsigned char cmosram[256];
int cccount=0;
int keydelay,keydelay2;
int armirq=0;
int output=0;
int firstins;

#define USER       0
#define FIQ        1
#define IRQ        2
#define SUPERVISOR 3

#define NFSET ((armregs[15]&0x80000000)?1:0)
#define ZFSET ((armregs[15]&0x40000000)?1:0)
#define CFSET ((armregs[15]&0x20000000)?1:0)
#define VFSET ((armregs[15]&0x10000000)?1:0)

#define NFLAG 0x80000000
#define ZFLAG 0x40000000
#define CFLAG 0x20000000
#define VFLAG 0x10000000
#define IFLAG 0x08000000

#define RD ((opcode>>12)&0xF)
#define RN ((opcode>>16)&0xF)
#define RM (opcode&0xF)

#define MULRD ((opcode>>16)&0xF)
#define MULRN ((opcode>>12)&0xF)
#define MULRS ((opcode>>8)&0xF)
#define MULRM (opcode&0xF)

#define GETADDR(r) ((r==15)?(armregs[15]&0x3FFFFFC):armregs[r])
#define LOADREG(r,v) if (r==15) { armregs[15]=(armregs[15]&0xFC000003)|((v+4)&0x3FFFFFC); refillpipeline(); } else armregs[r]=v;
#define GETREG(r) ((r==15) ? armregs[15]+4 : armregs[r])
#define LDRRESULT(a,v) ((a&3)?(v>>((a&3)<<3))|(v<<(((a&3)^3)<<3)):v)

int ins=0;

/*0=i/o, 1=all, 2=r/o, 3=os r/o, 4=super only, 5=read mem, write io*/
/*0=user, 1=os, 2=super*/
int modepritabler[3][6]=
{
        {0,1,1,0,0,1},
        {0,1,1,1,0,1},
        {0,1,1,1,1,1}
};
int modepritablew[3][6]=
{
        {0,1,0,0,0,0},
        {0,1,1,0,0,0},
        {0,1,1,1,1,0}
};

int mode;
int osmode=0;

void updatemode(int m)
{
        int c;
        usrregs[15]=&armregs[15];
        switch (mode) /*Store back registers*/
        {
                case USER:
                for (c=8;c<15;c++) userregs[c]=armregs[c];
                break;
                case IRQ:
                for (c=8;c<13;c++) userregs[c]=armregs[c];
                irqregs[0]=armregs[13];
                irqregs[1]=armregs[14];
                break;
                case FIQ:
                for (c=8;c<15;c++) fiqregs[c]=armregs[c];
                break;
                case SUPERVISOR:
                for (c=8;c<13;c++) userregs[c]=armregs[c];
                superregs[0]=armregs[13];
                superregs[1]=armregs[14];
                break;
        }
        mode=m;
        switch (m)
        {
                case USER:
                for (c=8;c<15;c++) armregs[c]=userregs[c];
                memmode=osmode;
                for (c=0;c<15;c++) usrregs[c]=&armregs[c];
                break;
                case IRQ:
                for (c=8;c<13;c++) armregs[c]=userregs[c];
                armregs[13]=irqregs[0];
                armregs[14]=irqregs[1];
                for (c=0;c<13;c++) usrregs[c]=&armregs[c];
                for (c=13;c<15;c++) usrregs[c]=&userregs[c];
                memmode=2;
                break;
                case FIQ:
                for (c=8;c<15;c++) armregs[c]=fiqregs[c];
                for (c=0;c<8;c++)  usrregs[c]=&armregs[c];
                for (c=8;c<15;c++) usrregs[c]=&userregs[c];
                memmode=2;
                break;
                case SUPERVISOR:
                for (c=8;c<13;c++) armregs[c]=userregs[c];
                armregs[13]=superregs[0];
                armregs[14]=superregs[1];
                for (c=0;c<13;c++) usrregs[c]=&armregs[c];
                for (c=13;c<15;c++) usrregs[c]=&userregs[c];
                memmode=2;
                break;
        }
}

unsigned long pccache,*pccache2;
int stmlookup[256];
#define countbits(c) countbitstable[c]
int countbitstable[65536];
void resetarm()
{
        int c,d,exec,data;
        unsigned long rotval,rotamount;
        for (c=0;c<256;c++)
        {
                stmlookup[c]=0;
                for (d=0;d<8;d++)
                {
                        if (c&(1<<d)) stmlookup[c]+=4;
                }
        }
        for (c=0;c<65536;c++)
        {
                countbitstable[c]=0;
                for (d=0;d<16;d++)
                {
                        if (c&(1<<d)) countbitstable[c]+=4;
                }
        }
//        if (!olog)
//           olog=fopen("armlog.txt","wt");
        pccache=0xFFFFFFFF;
        updatemode(SUPERVISOR);
        for (c=0;c<16;c++)
        {
                for (d=0;d<16;d++)
                {
                        armregs[15]=d<<28;
                        switch (c)
                        {
                                case 0:  /*EQ*/ exec=ZFSET; break;
                                case 1:  /*NE*/ exec=!ZFSET; break;
                                case 2:  /*CS*/ exec=CFSET; break;
                                case 3:  /*CC*/ exec=!CFSET; break;
                                case 4:  /*MI*/ exec=NFSET; break;
                                case 5:  /*PL*/ exec=!NFSET; break;
                                case 6:  /*VS*/ exec=VFSET; break;
                                case 7:  /*VC*/ exec=!VFSET; break;
                                case 8:  /*HI*/ exec=(CFSET && !ZFSET); break;
                                case 9:  /*LS*/ exec=(!CFSET || ZFSET); break;
                                case 10: /*GE*/ exec=(NFSET == VFSET); break;
                                case 11: /*LT*/ exec=(NFSET != VFSET); break;
                                case 12: /*GT*/ exec=(!ZFSET && (NFSET==VFSET)); break;
                                case 13: /*LE*/ exec=(ZFSET || (NFSET!=VFSET)); break;
                                case 14: /*AL*/ exec=1; break;
                                case 15: /*NV*/ exec=0; break;
                        }
                        flaglookup[c][d]=exec;
                }
        }

        for (data=0;data<4096;data++)
        {
                rotval=data&0xFF;
                rotamount=((data>>8)&0xF)<<1;
                rotval=(rotval>>rotamount)|(rotval<<(32-rotamount));
                rotatelookup[data]=rotval;
        }

        armregs[15]=0x0C00000B;
        mode=3;
        memmode=2;
        firstins=1;
        memstat[0]=1;
        mempoint[0]=rom;
        refillpipeline2();
        resetcp15();
        resetfpa();
        resetics();
        resetarcrom();
}

int indumpregs=0;

void dumpregs()
{
        int c;
        char s[2048];
        FILE *f;
//        char *t;
        if (indumpregs) return;
        indumpregs=1;
//        t=0;
//        *t=0xFF;
/*        f=fopen("ram2.dmp","wb");
        for (c=0x0000;c<0x8000;c++)
            putc(readmemb(c),f);
        fclose(f);*/
/*        f=fopen("ic262.dmp","wb");
        fwrite(&rom[0x40000],0x80000,1,f);
        fclose(f);*/
/*        f=fopen("modules.dmp","wb");
        for (c=0x0000;c<0x80000;c++)
            putc(readmemb(c+0x1800000),f);
        fclose(f);*/
/*        f=fopen("heap.dmp","wb");
        for (c=0x0000;c<0x8000;c++)
            putc(readmemb(c+0x1C00000),f);
        fclose(f);*/
/*        f=fopen("ram.dmp","wb");
        for (c=0x0000;c<0x100000;c++)
            putc(readmemb(c+0x8000),f);
        fclose(f);
        f=fopen("realram.dmp","wb");
        fwrite(ram,0x800000,1,f);
        fclose(f);*/
/*        f=fopen("rom.dmp","wb");
        fwrite(rom,0x200000,1,f);
        fclose(f);*/
//        fclose(olog);
        sprintf(s,"R 0=%08X R 4=%08X R 8=%08X R12=%08X\nR 1=%08X R 5=%08X R 9=%08X R13=%08X\nR 2=%08X R 6=%08X R10=%08X R14=%08X\nR 3=%08X R 7=%08X R11=%08X R15=%08X\n%i %08X %08X\nf 8=%08X f 9=%08X f10=%08X f11=%08X\nf12=%08X f13=%08X f14=%08X",armregs[0],armregs[4],armregs[8],armregs[12],armregs[1],armregs[5],armregs[9],armregs[13],armregs[2],armregs[6],armregs[10],armregs[14],armregs[3],armregs[7],armregs[11],armregs[15],ins,oldpc,oldoldpc,fiqregs[8],fiqregs[9],fiqregs[10],fiqregs[11],fiqregs[12],fiqregs[13],fiqregs[14]);
//        MessageBox(NULL,s,"ARM register dump",MB_OK);
        rpclog("%s",s);
//        printf("f12=%08X %08X %08X  ",fiqregs[12],oldpc,oldoldpc);
//        printf("PC =%07X ins=%i\n",PC,ins);
//        for (c=0;c<0x80000;c++)
//            putc(((unsigned char *)ram)[c],f);
//        fclose(f);
//        f=fopen("zeropage.dmp","wb");
//        fwrite(ram,0x8000,1,f);
//        fclose(f);
//        f=fopen("cmos.bin","wb");
//        fwrite(cmosram,256,1,f);
//        fclose(f);*/
/*        f=fopen("highpage.dmp","wb");
        for (c=0x1F00000;c<0x1F08000;c++)
            putc(readmemb(c),f);
        fclose(f);
        f=fopen("midpage.dmp","wb");
        for (c=0x1810000;c<0x1818000;c++)
            putc(readmemb(c),f);
        fclose(f);*/
//        dumpvid();
//        printf("VIDC=%08X\n",vidcr[0xE0>>2]);
        indumpregs=0;
}

#define checkneg(v) (v&0x80000000)
#define checkpos(v) !(v&0x80000000)

static inline void setadd(uint32_t op1, uint32_t op2, uint32_t res)
{
        armregs[15]&=0xFFFFFFF;
        if (!res)                           armregs[15]|=ZFLAG;
        else if (checkneg(res))             armregs[15]|=NFLAG;
        if (res<op1)                        armregs[15]|=CFLAG;
        if ((op1^res)&(op2^res)&0x80000000) armregs[15]|=VFLAG;
}

static inline void setsub(uint32_t op1, uint32_t op2, uint32_t res)
{
        armregs[15]&=0xFFFFFFF;
        if (!res)                           armregs[15]|=ZFLAG;
        else if (checkneg(res))             armregs[15]|=NFLAG;
        if (res<=op1)                       armregs[15]|=CFLAG;
        if ((op1^op2)&(op1^res)&0x80000000) armregs[15]|=VFLAG;
}

static inline void setsbc(uint32_t op1, uint32_t op2, uint32_t res)
{
        armregs[15]&=0xFFFFFFF;
        if (!res)                           armregs[15]|=ZFLAG;
        else if (checkneg(res))             armregs[15]|=NFLAG;
        if ((checkneg(op1) && checkpos(op2)) ||
            (checkneg(op1) && checkpos(res)) ||
            (checkpos(op2) && checkpos(res)))  armregs[15]|=CFLAG;
        if ((checkneg(op1) && checkpos(op2) && checkpos(res)) ||
            (checkpos(op1) && checkneg(op2) && checkneg(res)))
            armregs[15]|=VFLAG;
}

static inline void setadc(uint32_t op1, uint32_t op2, uint32_t res)
{
        armregs[15]&=0xFFFFFFF;
        if ((checkneg(op1) && checkneg(op2)) ||
            (checkneg(op1) && checkpos(res)) ||
            (checkneg(op2) && checkpos(res)))  armregs[15]|=CFLAG;
        if ((checkneg(op1) && checkneg(op2) && checkpos(res)) ||
            (checkpos(op1) && checkpos(op2) && checkneg(res)))
            armregs[15]|=VFLAG;
        if (!res)                          armregs[15]|=ZFLAG;
        else if (checkneg(res))            armregs[15]|=NFLAG;
}
/*
inline void setadd(unsigned long op1, unsigned long op2, unsigned long res)
{
        armregs[15]&=0xFFFFFFF;
        if ((checkneg(op1) && checkneg(op2)) ||
            (checkneg(op1) && checkpos(res)) ||
            (checkneg(op2) && checkpos(res)))  armregs[15]|=CFLAG;
        if ((checkneg(op1) && checkneg(op2) && checkpos(res)) ||
            (checkpos(op1) && checkpos(op2) && checkneg(res)))
            armregs[15]|=VFLAG;
        if (!res)                          armregs[15]|=ZFLAG;
        else if (checkneg(res))            armregs[15]|=NFLAG;
}

inline void setsub(unsigned long op1, unsigned long op2, unsigned long res)
{
        char s[80];
        armregs[15]&=0xFFFFFFF;
        if ((checkneg(op1) && checkpos(op2)) ||
            (checkneg(op1) && checkpos(res)) ||
            (checkpos(op2) && checkpos(res)))  armregs[15]|=CFLAG;
        if (!res)               armregs[15]|=ZFLAG;
        else if (checkneg(res)) armregs[15]|=NFLAG;
        if ((checkneg(op1) && checkpos(op2) && checkpos(res)) ||
            (checkpos(op1) && checkneg(op2) && checkneg(res)))
            armregs[15]|=VFLAG;
}*/

inline void setzn(unsigned long op)
{
        armregs[15]&=0x3FFFFFFF;
        if (!op)               armregs[15]|=ZFLAG;
        else if (checkneg(op)) armregs[15]|=NFLAG;
}
char err2[512];

#define shift(o)  ((o&0xFF0)?shift3(o):armregs[RM])
#define shift2(o) ((o&0xFF0)?shift4(o):armregs[RM])

#define uint32_t unsigned long

static inline uint32_t shift3(uint32_t opcode)
{
        uint32_t shiftmode=(opcode>>5)&3;
        uint32_t shiftamount=(opcode>>7)&31;
        uint32_t temp;
        int cflag=CFSET;
        if (!(opcode&0xFF0)) return armregs[RM];
        if (opcode&0x10)
        {
                shiftamount=armregs[(opcode>>8)&15]&0xFF;
                if (shiftmode==3)
                   shiftamount&=0x1F;
                cycles--;
        }
        temp=armregs[RM];
//        if (RM==15)        temp+=4;
        if (opcode&0x100000 && shiftamount) armregs[15]&=~CFLAG;
        switch (shiftmode)
        {
                case 0: /*LSL*/
                if (!shiftamount) return temp;
                if (shiftamount==32)
                {
                        if (temp&1 && opcode&0x100000) armregs[15]|=CFLAG;
                        return 0;
                }
                if (shiftamount>32) return 0;
                if (opcode&0x100000)
                {
                        if ((temp<<(shiftamount-1))&0x80000000) armregs[15]|=CFLAG;
                }
                return temp<<shiftamount;

                case 1: /*LSR*/
                if (!shiftamount && !(opcode&0x10))
                {
                        shiftamount=32;
                }
                if (!shiftamount) return temp;
                if (shiftamount==32)
                {
                        if (temp&0x80000000 && opcode&0x100000) armregs[15]|=CFLAG;
                        else if (opcode&0x100000)               armregs[15]&=~CFLAG;
                        return 0;
                }
                if (shiftamount>32) return 0;
                if (opcode&0x100000)
                {
                        if ((temp>>(shiftamount-1))&1) armregs[15]|=CFLAG;
                }
                return temp>>shiftamount;

                case 2: /*ASR*/
                if (!shiftamount)
                {
                        if (opcode&0x10) return temp;
                }
                if (shiftamount>=32 || !shiftamount)
                {
                        if (temp&0x80000000 && opcode&0x100000) armregs[15]|=CFLAG;
                        else if (opcode&0x100000)               armregs[15]&=~CFLAG;
                        if (temp&0x80000000) return 0xFFFFFFFF;
                        return 0;
                }
                if (opcode&0x100000)
                {
                        if (((int)temp>>(shiftamount-1))&1) armregs[15]|=CFLAG;
                }
                return (int)temp>>shiftamount;

                case 3: /*ROR*/
                if (opcode&0x100000) armregs[15]&=~CFLAG;
                if (!shiftamount && !(opcode&0x10))
                {
                        if (opcode&0x100000 && temp&1) armregs[15]|=CFLAG;
                        return (((cflag)?1:0)<<31)|(temp>>1);
                }
                if (!shiftamount)
                {
                        if (opcode&0x100000) armregs[15]|=cflag;
                        return temp;
                }
                if (!(shiftamount&0x1F))
                {
                        if (opcode&0x100000 && temp&0x80000000) armregs[15]|=CFLAG;
                        return temp;
                }
                if (opcode&0x100000)
                {
                        if (((temp>>shiftamount)|(temp<<(32-shiftamount)))&0x80000000) armregs[15]|=CFLAG;
                }
                return (temp>>shiftamount)|(temp<<(32-shiftamount));
                break;
                default:
                sprintf(err2,"Shift mode %i amount %i\n",shiftmode,shiftamount);
                error("%s",err2);
                dumpregs();
                exit(-1);
        }
}
inline unsigned shift4(unsigned opcode)
{
        unsigned shiftmode=(opcode>>5)&3;
        unsigned shiftamount=(opcode>>7)&31;
        uint32_t temp;
        int cflag=CFSET;
        if (!(opcode&0xFF0)) return armregs[RM];
        if (opcode&0x10)
        {
                shiftamount=armregs[(opcode>>8)&15]&0xFF;
                if (shiftmode==3)
                   shiftamount&=0x1F;
//                cycles--;
        }
        temp=armregs[RM];
//        if (RM==15) temp+=4;
        switch (shiftmode)
        {
                case 0: /*LSL*/
                if (!shiftamount)    return temp;
                if (shiftamount>=32) return 0;
                return temp<<shiftamount;

                case 1: /*LSR*/
                if (!shiftamount && !(opcode&0x10))    return 0;
                if (shiftamount>=32) return 0;
                return temp>>shiftamount;

                case 2: /*ASR*/
                if (!shiftamount && !(opcode&0x10)) shiftamount=32;
                if (shiftamount>=32)
                {
                        if (temp&0x80000000)
                           return 0xFFFFFFFF;
                        return 0;
                }
                return (int)temp>>shiftamount;

                case 3: /*ROR*/
                if (!shiftamount && !(opcode&0x10)) return (((cflag)?1:0)<<31)|(temp>>1);
                if (!shiftamount)                   return temp;
                return (temp>>shiftamount)|(temp<<(32-shiftamount));
                break;

                default:
                sprintf(err2,"Shift2 mode %i amount %i\n",shiftmode,shiftamount);
                error("%s",err2);
                dumpregs();
                exit(-1);
        }
}

inline unsigned rotate(unsigned data)
{
        unsigned long rotval;
        rotval=rotatelookup[data&4095];
        if (data&0x100000 && data&0xF00)
        {
                if (rotval&0x80000000) armregs[15]|=CFLAG;
                else                   armregs[15]&=~CFLAG;
        }
        return rotval;
}

#define rotate2(v) rotatelookup[v&4095]

inline unsigned shiftmem(unsigned opcode)
{
        unsigned shiftmode=(opcode>>5)&3;
        unsigned shiftamount=(opcode>>7)&31;
        uint32_t temp;
        int cflag=CFSET;
        if (!(opcode&0xFF0)) return armregs[RM];
        if (opcode&0x10)
        {
                rpclog("Shift by register on memory shift!!! %08X\n",PC);
                exit(-1);
        }
        temp=armregs[RM];
        switch (shiftmode)
        {
                case 0: /*LSL*/
                if (!shiftamount)    return temp;
                return temp<<shiftamount;

                case 1: /*LSR*/
                if (!shiftamount)    return 0;
                return temp>>shiftamount;

                case 2: /*ASR*/
                if (!shiftamount)
                {
                        if (temp&0x80000000)
                           return 0xFFFFFFFF;
                        return 0;
                }
                return (int)temp>>shiftamount;

                case 3: /*ROR*/
                if (!shiftamount) return (((cflag)?1:0)<<31)|(temp>>1);
                return (temp>>shiftamount)|(temp<<(32-shiftamount));
                break;

                default:
                sprintf(err2,"Shiftmem mode %i amount %i\n",shiftmode,shiftamount);
                error("%s",err2);
                dumpregs();
                exit(-1);
        }
}

int ldrlookup[4]={0,8,16,24};

#define ldrresult(v,a) ((v>>ldrlookup[addr&3])|(v<<(32-ldrlookup[addr&3])))

/*unsigned long ldrresult(unsigned long val, unsigned long addr)
{
//        if (addr&3) printf("Unaligned access\n");
//        return val;
        switch (addr&3)
        {
                case 0:
                return val;
                case 1:
                return (val>>8)|(val<<24);
                case 2:
                return (val>>16)|(val<<16);
                case 3:
                return (val>>24)|(val<<8);
        }
}*/

#define undefined()\
                                        templ=armregs[15]-4; \
                                        armregs[15]|=3;\
                                        updatemode(SUPERVISOR);\
                                        armregs[14]=templ;\
                                        armregs[15]&=0xFC000003;\
                                        armregs[15]|=0x08000008;\
                                        refillpipeline();\
                                        cycles--

int accc=0;
FILE *slogfile;
int ins;
/*int genpol=8,genpoll=8;
int redogenpol(int notnew)
{
        int cyc=genpoll;
        if (notnew) cyc-=genpol;
//        rpclog("Genpol %i ",cyc);
        ioc.timerc[0]-=(cyc<<1);
        ioc.timerc[1]-=(cyc<<1);
        if ((ioc.timerc[0]<0) || (ioc.timerc[1]<0)) updateioctimers();
        soundtime-=cyc;
        while (soundtime<0)
        {
                pollsound();
                if (!speed)        soundtime+=32;
                else if (speed==1) soundtime+=48;
                else               soundtime+=96;
        }
        if (inreadop)
        {
                fdicount-=cyc;
                if (fdicount<0)
                {
                        fdicount+=8;
                        fdinextbit();
                }
        }
        genpol=10000000;
        if ((ioc.timerc[0]<<1)<genpol && ioc.timerc[0])
        {
                genpol=ioc.timerc[0]<<1;
                if (!genpol) genpol++;
        }
        if ((ioc.timerc[1]<<1)<genpol && ioc.timerc[1])
        {
                genpol=ioc.timerc[1]<<1;
                if (!genpol) genpol++;
        }
        if (soundtime<genpol && soundtime) genpol=soundtime;
        if (inreadop && fdicount<genpol) genpol=fdicount;
        genpoll=genpol;
//        rpclog("%i %i %i %i\n",genpol,ioc.timerc[0],ioc.timerc[1],soundtime);
}*/

#define readmemfff(addr,opcode) \
                        if ((addr>>12)==pccache) \
                           opcode=pccache2[(addr&0xFFF)>>2]; \
                        else \
                        { \
                                templ2=addr>>12; \
                                templ=memstat[addr>>12]; \
                                if (modepritabler[memmode][templ/*memstat[PC>>15]*/]) \
                                { \
                                        pccache=templ2; \
                                        pccache2=mempoint[templ2/*PC>>15*/]; \
                                        opcode=mempoint[templ2/*PC>>15*/][(addr&0xFFF)>>2]; \
                                } \
                                else \
                                { \
                                        opcode=readmemf(addr); \
                                        pccache=0xFFFFFFFF; \
                                } \
                        }

void refillpipeline()
{
        unsigned long templ,templ2,addr=PC-4;
        readmemfff(addr,opcode2);
        addr+=4;
        readmemfff(addr,opcode3);
}

void refillpipeline2()
{
        unsigned long templ,templ2,addr=PC-8;
        readmemfff(addr,opcode2);
        addr+=4;
        readmemfff(addr,opcode3);
//        if (!olog) olog=fopen("armlog.txt","wt");
//        opcode=readmeml(PC-8);
//        opcode2=readmemff(PC-8);
//        opcode3=readmemff(PC-4);
//        cycles-=3;
//        sprintf(bigs,"Fetched - %08X %07X, %08X %07X\n",opcode2,PC-4,opcode3,PC);
//        fputs(bigs,olog);
}

int samplesperline;
unsigned long opc;
int framecycs;
void execarm(int cycs)
{
        unsigned long templ,templ2,mask,addr,addr2,*rn;
        int c,cyc,oldcyc,oldcyc2;
//        unsigned long oldr15[2];
//        FILE *f;
        char s[80];
//        char bigs[1024];
        int linecyc;
        int reloadedbase;
        cycles+=cycs;
//        rpclog("Execarm %i %i\n",cycles,cycs);
        samplesperline=0;
        while (cycles>0)
        {
                oldcyc=cycles;
                linecyc=vidcgetcycs();
                framecycs+=linecyc;
//                rpclog("Cyclesperline %i  %i %i  ",linecyc,ioc.timerc[0],ioc.timerc[1]);
                while (linecyc>0)
                {
                        opcode=opcode2;
                        opcode2=opcode3;
                        oldcyc2=cycles;
                        opc=PC;
                        if ((PC>>12)==pccache)
                           opcode3=pccache2[(PC&0xFFF)>>2];
                        else
                        {
                                templ2=PC>>12;
                                templ=memstat[PC>>12];
//                                rpclog("%07X : templ2 %08X templ %08X\n",PC,templ2,templ);
                                if (modepritabler[memmode][templ/*memstat[PC>>15]*/])
                                {
                                        pccache=templ2;//PC>>15;
                                        pccache2=mempoint[templ2/*PC>>15*/];
                                        opcode3=mempoint[templ2/*PC>>15*/][(PC&0xFFF)>>2];
                                }
                                else
                                {
                                        opcode3=readmemf(PC);
                                        pccache=0xFFFFFFFF;
                                }
                        }
                        if (!((PC)&0xC)) cycles--;
/*                        if (opcode==0xE35F0000)
                        {
                                sprintf(bigs,"Weird opcode executed at %07X\n",PC);
                                fputs(bigs,olog);
                        }*/
                        if (flaglookup[opcode>>28][armregs[15]>>28] && !prefabort)
                        {
//                                if ((PC>=0x1800000 && PC<0x1840000) && (((opcode>>20)&0xE4)==0x84) && !(opcode&0x8000))
//                                   rpclog("%08X at %07X\n",opcode,PC);
/*                                if (((opcode>>20)&0xFF)<0x20 && (RN==15 || RM==15) && (opcode&0x10))
                                {
                                        rpclog("Register shift on opcode %08X %07X\n",opcode,PC);
//                                        templ=(opcode>>20)&0xFF;//3D???
//                                        if (templ!=8 && templ!=9 && templ!=0x13 && templ!=0x1A && templ!=0x1B && templ!=0x33 && templ!=0x11 && templ!=0x25 && templ!=0x3D && templ!=0x39 && templ!=0x24 && templ!=0x28)
//                                        if (templ&1 && !(armregs[15]&3))
//                                           rpclog("USER Opcode RD=15 %08X %07X\n",opcode,PC);
                                }*/
                                switch ((opcode>>20)&0xFF)
                                {
                                        case 0x00: /*AND reg*/
                                        if (((opcode&0xF0)==0x90)) /*MUL*/
                                        {
                                                armregs[MULRD]=(armregs[MULRM])*(armregs[MULRS]);
                                                if (MULRD==MULRM) armregs[MULRD]=0;
                                                cycles-=17;
                                        }
                                        else
                                        {
                                                if (RD==15)
                                                {
                                                        templ=shift2(opcode);
                                                        armregs[15]=(((GETADDR(RN)&templ)+4)&0x3FFFFFC)|(armregs[15]&0xFC000003);
                                                        refillpipeline();
                                                }
                                                else
                                                {
                                                        templ=shift2(opcode);
                                                        armregs[RD]=GETADDR(RN)&templ;
                                                }
                                                cycles--;
                                        }
                                        break;
                                        case 0x01: /*ANDS reg*/
                                        if (((opcode&0xF0)==0x90)) /*MULS*/
                                        {
                                                armregs[MULRD]=(armregs[MULRM])*(armregs[MULRS]);
                                                if (MULRD==MULRM) armregs[MULRD]=0;
                                                setzn(armregs[MULRD]);
                                                cycles-=17;
                                        }
                                        else
                                        {
                                                if (RD==15)
                                                {
                                                        templ=shift2(opcode);
                                                        armregs[15]=(GETADDR(RN)&templ)+4;
                                                        refillpipeline();
                                                }
                                                else
                                                {
                                                        templ=shift(opcode);
                                                        armregs[RD]=GETADDR(RN)&templ;
                                                        setzn(armregs[RD]);
                                                }
                                                cycles--;
                                        }
                                        break;

                                        case 0x02: /*EOR reg*/
                                        if (((opcode&0xF0)==0x90)) /*MLA*/
                                        {
                                                armregs[MULRD]=((armregs[MULRM])*(armregs[MULRS]))+armregs[MULRN];
                                                if (MULRD==MULRM) armregs[MULRD]=0;
                                                cycles-=17;
                                        }
                                        else
                                        {
                                                if (RD==15)
                                                {
                                                        templ=shift2(opcode);
                                                        armregs[15]=(((GETADDR(RN)^templ)+4)&0x3FFFFFC)|(armregs[15]&0xFC000003);
                                                        refillpipeline();
                                                }
                                                else
                                                {
                                                        templ=shift2(opcode);
                                                        armregs[RD]=GETADDR(RN)^templ;
                                                }
                                                cycles--;
                                        }
                                        break;
                                        case 0x03: /*EORS reg*/
                                        if (((opcode&0xF0)==0x90)) /*MLAS*/
                                        {
                                                armregs[MULRD]=((armregs[MULRM])*(armregs[MULRS]))+armregs[MULRN];
                                                if (MULRD==MULRM) armregs[MULRD]=0;
                                                setzn(armregs[MULRD]);
                                                cycles-=17;
                                        }
                                        else
                                        {
                                                if (RD==15)
                                                {
                                                        templ=shift2(opcode);
                                                        armregs[15]=(GETADDR(RN)^templ)+4;
                                                        refillpipeline();
                                                }
                                                else
                                                {
                                                        templ=shift(opcode);
                                                        armregs[RD]=GETADDR(RN)^templ;
                                                        setzn(armregs[RD]);
                                                }
                                                cycles--;
                                        }
                                        break;

                                        case 0x04: /*SUB reg*/
                                        if (RD==15)
                                        {
                                                templ=shift2(opcode);
                                                armregs[15]=(((GETADDR(RN)-templ)+4)&0x3FFFFFC)|(armregs[15]&0xFC000003);
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=shift2(opcode);
                                                armregs[RD]=GETADDR(RN)-templ;
                                        }
                                        cycles--;
                                        break;
                                        case 0x05: /*SUBS reg*/
                                        if (RD==15)
                                        {
                                                templ=shift2(opcode);
                                                armregs[15]=(GETADDR(RN)-templ)+4;
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=shift2(opcode);
                                                setsub(GETADDR(RN),templ,GETADDR(RN)-templ);
                                                armregs[RD]=GETADDR(RN)-templ;
                                        }
                                        cycles--;
                                        break;

                                        case 0x06: /*RSB reg*/
                                        if (RD==15)
                                        {
                                                templ=shift2(opcode);
                                                armregs[15]=(((templ-GETADDR(RN))+4)&0x3FFFFFC)|(armregs[15]&0xFC000003);
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=shift2(opcode);
                                                armregs[RD]=templ-GETADDR(RN);
                                        }
                                        cycles--;
                                        break;
                                        case 0x07: /*RSBS reg*/
                                        if (RD==15)
                                        {
                                                templ=shift2(opcode);
                                                armregs[15]=(templ-GETADDR(RN))+4;
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=shift2(opcode);
                                                setsub(templ,GETADDR(RN),templ-GETADDR(RN));
                                                armregs[RD]=templ-GETADDR(RN);
                                        }
                                        cycles--;
                                        break;

                                        case 0x08: /*ADD reg*/
                                        if (RD==15)
                                        {
                                                templ=shift2(opcode);
        //                                        printf("R15=%08X+%08X+4=",GETADDR(RN),templ);
                                                armregs[15]=((GETADDR(RN)+templ+4)&0x3FFFFFC)|(armregs[15]&0xFC000003);
                                                refillpipeline();
        //                                        printf("%08X\n",armregs[15]);
                                        }
                                        else
                                        {
                                                templ=shift2(opcode);
                                                armregs[RD]=GETADDR(RN)+templ;
                                        }
                                        cycles--;
                                        break;
                                        case 0x09: /*ADDS reg*/
                                        if (RD==15)
                                        {
                                                templ=shift2(opcode);
        //                                        printf("R15=%08X+%08X+4=",GETADDR(RN),templ);
                                                armregs[15]=GETADDR(RN)+templ+4;
                                                refillpipeline();
        //                                        printf("%08X\n",armregs[15]);
                                        }
                                        else
                                        {
                                                templ=shift2(opcode);
                                                setadd(GETADDR(RN),templ,GETADDR(RN)+templ);
        //                                        printf("ADDS %08X+%08X = ",GETADDR(RN),templ);
                                                armregs[RD]=GETADDR(RN)+templ;
        //                                        printf("%08X\n",armregs[RD]);
        //                                        setzn(templ);
                                        }
                                        cycles--;
                                        break;
                                
                                        case 0x0A: /*ADC reg*/
                                        if (RD==15)
                                        {
                                                templ2=CFSET;
                                                templ=shift2(opcode);
                                                armregs[15]=((GETADDR(RN)+templ+templ2+4)&0x3FFFFFC)|(armregs[15]&0xFC000003);
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ2=CFSET;
                                                templ=shift2(opcode);
                                                armregs[RD]=GETADDR(RN)+templ+templ2;
                                        }
                                        cycles--;
                                        break;
                                        case 0x0B: /*ADCS reg*/
                                        if (RD==15)
                                        {
                                                templ2=CFSET;
                                                templ=shift2(opcode);
                                                armregs[15]=GETADDR(RN)+templ+templ2+4;
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ2=CFSET;
                                                templ=shift2(opcode);
                                                setadc(GETADDR(RN),templ,GETADDR(RN)+templ+templ2);
                                                armregs[RD]=GETADDR(RN)+templ+templ2;
                                        }
                                        cycles--;
                                        break;

                                        case 0x0C: /*SBC reg*/
                                        templ2=(CFSET)?0:1;
                                        if (RD==15)
                                        {
                                                templ=shift2(opcode);
                                                armregs[15]=(((GETADDR(RN)-(templ+templ2))+4)&0x3FFFFFC)|(armregs[15]&0xFC000003);
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=shift2(opcode);
                                                armregs[RD]=GETADDR(RN)-(templ+templ2);
                                        }
                                        cycles--;
                                        break;
                                        case 0x0D: /*SBCS reg*/
                                        templ2=(CFSET)?0:1;
                                        if (RD==15)
                                        {
                                                templ=shift2(opcode);
                                                armregs[15]=(GETADDR(RN)-(templ+templ2))+4;
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=shift2(opcode);
                                                setsbc(GETADDR(RN),templ,GETADDR(RN)-(templ+templ2));
                                                armregs[RD]=GETADDR(RN)-(templ+templ2);
                                        }
                                        cycles--;
                                        break;
                                        case 0x0E: /*RSC reg*/
                                        templ2=(CFSET)?0:1;
                                        if (RD==15)
                                        {
                                                templ=shift2(opcode);
                                                armregs[15]=(((templ-(GETADDR(RN)+templ2))+4)&0x3FFFFFC)|(armregs[15]&0xFC000003);
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=shift2(opcode);
                                                armregs[RD]=templ-(GETADDR(RN)+templ2);
                                        }
                                        cycles--;
                                        break;
                                        case 0x0F: /*RSCS reg*/
                                        templ2=(CFSET)?0:1;
                                        if (RD==15)
                                        {
                                                templ=shift2(opcode);
                                                armregs[15]=(templ-(GETADDR(RN)+templ2))+4;
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=shift2(opcode);
                                                setsbc(templ,GETADDR(RN),templ-(GETADDR(RN)+templ2));
                                                armregs[RD]=templ-(GETADDR(RN)+templ2);
                                        }
                                        cycles--;
                                        break;

                                        case 0x10: /*SWP word*/
                                        if (arm3 && (opcode&0xF0)==0x90)
                                        {
                                                addr=GETADDR(RN);
                                                templ=GETREG(RM);
                                                LOADREG(RD,readmeml(addr));
                                                if (!databort) writememl(addr,templ);
                                                cycles-=3;
                                        }
                                        else
                                        {
                                                #if 0
                                                dumpregs();
                                                exit(-1);
                                                /*No undefined on ARM2?*/
                                                templ=armregs[15]-4;
                                                armregs[15]|=3;
                                                updatemode(SUPERVISOR);
                                                armregs[14]=templ;
                                                armregs[15]&=0xFC000003;
                                                armregs[15]|=0x08000008;
                                                cycles-=3;
                                                refillpipeline();
                                                #endif
                                        }
                                        break;
                                        
                                        case 0x11: /*TST reg*/
                                        if (RD==15)
                                        {
                                                if (armregs[15]&3)
                                                {
                                                        templ=armregs[15]&0x3FFFFFC;
                                                        armregs[15]=((GETADDR(RN)&shift2(opcode))&0xFC000003)|templ;
                                                }
                                                else
                                                {
                                                        templ=armregs[15]&0x0FFFFFFF;
                                                        armregs[15]=((GETADDR(RN)&shift2(opcode))&0xF0000000)|templ;
                                                }
                                        }
                                        else
                                        {
                                                setzn(GETADDR(RN)&shift(opcode));
                                        }
                                        cycles--;
                                        break;

                                        case 0x12:
                                        break;
                                        
                                        case 0x13: /*TEQ reg*/
                                        if (RD==15)
                                        {
                                                if (armregs[15]&3)
                                                {
                                                        templ=armregs[15]&0x3FFFFFC;
//                                                        if (PC<0x1800000) rpclog("%07X : TEQP - %08X %08X %08X  ",PC,armregs[15],GETADDR(RN),shift2(opcode));
                                                        armregs[15]=((GETADDR(RN)^shift2(opcode))&0xFC000003)|templ;
//                                                        if (PC<0x1800000) printf(" - %08X\n",armregs[15]);
                                                }
                                                else
                                                {
                                                        templ=armregs[15]&0x0FFFFFFF;
                                                        armregs[15]=((GETADDR(RN)^shift2(opcode))&0xF0000000)|templ;
                                                }
                                        }
                                        else
                                        {
                                                setzn(GETADDR(RN)^shift(opcode));
                                        }
                                        cycles--;
                                        break;
                                        
                                        case 0x14: /*SWPB*/
                                        if (arm3)
                                        {
                                                addr=armregs[RN];
                                                templ=GETREG(RM);
                                                LOADREG(RD,readmemb(addr));
                                                writememb(addr,templ);
                                                cycles-=3;
                                        }
                                        else
                                        {
                                                /*No undefined on ARM2?*/
/*                                                templ=armregs[15]-4;
                                                armregs[15]|=3;
                                                updatemode(SUPERVISOR);
                                                armregs[14]=templ;
                                                armregs[15]&=0xFC000003;
                                                armregs[15]|=0x08000008;
                                                cycles-=3;
                                                refillpipeline();*/
                                        }
                                        break;

                                        case 0x15: /*CMP reg*/
                                        if (RD==15)
                                        {
                                                if (armregs[15]&3)
                                                {
                                                        templ=armregs[15]&0x3FFFFFC;
                                                        armregs[15]=((GETADDR(RN)-shift2(opcode))&0xFC000003)|templ;
                                                }
                                                else
                                                {
                                                        templ=armregs[15]&0x0FFFFFFF;
                                                        armregs[15]=((GETADDR(RN)-shift2(opcode))&0xF0000000)|templ;
                                                }
                                        }
                                        else
                                           setsub(GETADDR(RN),shift(opcode),GETADDR(RN)-shift2(opcode));
                                        cycles--;
                                        break;

                                        case 0x17: /*CMN reg*/
                                        if (RD==15)
                                        {
                                                if (armregs[15]&3)
                                                {
                                                        templ=armregs[15]&0x3FFFFFC;
                                                        armregs[15]=((GETADDR(RN)+shift2(opcode))&0xFC000003)|templ;
                                                }
                                                else
                                                {
                                                        templ=armregs[15]&0x0FFFFFFF;
                                                        armregs[15]=((GETADDR(RN)+shift2(opcode))&0xF0000000)|templ;
                                                }
                                        }
                                        else
                                           setadd(GETADDR(RN),shift2(opcode),GETADDR(RN)+shift2(opcode));
                                        cycles--;
                                        break;

                                        case 0x18: /*ORR reg*/
                                        if (RD==15)
                                        {
                                                templ=shift2(opcode);
                                                armregs[15]=(((GETADDR(RN)|templ)+4)&0x3FFFFFC)|(armregs[15]&0xFC000003);
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=shift2(opcode);
                                                armregs[RD]=GETADDR(RN)|templ;
                                        }
                                        cycles--;
                                        break;
                                        case 0x19: /*ORRS reg*/
                                        if (RD==15)
                                        {
                                                templ=shift2(opcode);
                                                armregs[15]=(GETADDR(RN)|templ)+4;
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=shift(opcode);
                                                armregs[RD]=GETADDR(RN)|templ;
                                                setzn(armregs[RD]);
                                        }
                                        cycles--;
                                        break;

                                        case 0x1A: /*MOV reg*/
                                        if (RD==15)
                                        {
                                                armregs[15]=(armregs[15]&0xFC000003)|((shift2(opcode)+4)&0x3FFFFFC);
                                                refillpipeline();
                                        }
                                        else
                                           armregs[RD]=shift2(opcode);
                                        cycles--;
                                        break;
                                        case 0x1B: /*MOVS reg*/
                                        if (RD==15)
                                        {
                                                if (armregs[15]&3) armregs[15]=shift2(opcode)+4;
                                                else               armregs[15]=((shift2(opcode)+4)&0xF3FFFFFC)|(armregs[15]&0xC000003);
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                armregs[RD]=shift(opcode);
                                                setzn(armregs[RD]);
                                        }
                                        cycles--;
                                        break;

                                        case 0x1C: /*BIC reg*/
                                        if (RD==15)
                                        {
                                                templ=shift2(opcode);
                                                armregs[15]=(((GETADDR(RN)&~templ)+4)&0x3FFFFFC)|(armregs[15]&0xFC000003);
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=shift2(opcode);
                                                armregs[RD]=GETADDR(RN)&~templ;
                                        }
                                        cycles--;
                                        break;
                                        case 0x1D: /*BICS reg*/
                                        if (RD==15)
                                        {
                                                templ=shift2(opcode);
                                                armregs[15]=(GETADDR(RN)&~templ)+4;
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=shift(opcode);
                                                armregs[RD]=GETADDR(RN)&~templ;
                                                setzn(armregs[RD]);
                                        }
                                        cycles--;
                                        break;

                                        case 0x1E: /*MVN reg*/
                                        if (RD==15)
                                        {
                                                armregs[15]=(armregs[15]&0xFC000003)|(((~shift2(opcode))+4)&0x3FFFFFC);
                                                refillpipeline();
                                        }
                                        else
                                           armregs[RD]=~shift2(opcode);
                                        cycles--;
                                        break;
                                        case 0x1F: /*MVNS reg*/
                                        if (RD==15)
                                        {
                                                armregs[15]=(~shift2(opcode))+4;
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                armregs[RD]=~shift(opcode);
                                                setzn(armregs[RD]);
                                        }
                                        cycles--;
                                        break;

                                        case 0x20: /*AND imm*/
                                        if (RD==15)
                                        {
                                                templ=rotate2(opcode);
                                                armregs[15]=(((GETADDR(RN)&templ)+4)&0x3FFFFFC)|(armregs[15]&0xFC000003);
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=rotate2(opcode);
                                                armregs[RD]=GETADDR(RN)&templ;
                                        }
                                        cycles--;
                                        break;
                                        case 0x21: /*ANDS imm*/
                                        if (RD==15)
                                        {
                                                templ=rotate2(opcode);
                                                armregs[15]=(GETADDR(RN)&templ)+4;
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=rotate(opcode);
                                                armregs[RD]=GETADDR(RN)&templ;
                                                setzn(armregs[RD]);
                                        }
                                        cycles--;
                                        break;

                                        case 0x22: /*EOR imm*/
                                        if (RD==15)
                                        {
                                                templ=rotate2(opcode);
                                                armregs[15]=(((GETADDR(RN)^templ)+4)&0x3FFFFFC)|(armregs[15]&0xFC000003);
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=rotate2(opcode);
                                                armregs[RD]=GETADDR(RN)^templ;
                                        }
                                        cycles--;
                                        break;
                                        case 0x23: /*EORS imm*/
                                        if (RD==15)
                                        {
                                                templ=rotate2(opcode);
                                                armregs[15]=(GETADDR(RN)^templ)+4;
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=rotate(opcode);
                                                armregs[RD]=GETADDR(RN)^templ;
                                                setzn(armregs[RD]);
                                        }
                                        cycles--;
                                        break;

                                        case 0x24: /*SUB imm*/
                                        if (RD==15)
                                        {
                                                templ=rotate2(opcode);
                                                armregs[15]=(((GETADDR(RN)-templ)+4)&0x3FFFFFC)|(armregs[15]&0xFC000003);
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=rotate2(opcode);
                                                armregs[RD]=GETADDR(RN)-templ;
                                        }
                                        cycles--;
                                        break;
                                        case 0x25: /*SUBS imm*/
                                        if (RD==15)
                                        {
                                                templ=rotate2(opcode);
                                                armregs[15]=(GETADDR(RN)-templ)+4;
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=rotate2(opcode);
                                                setsub(GETADDR(RN),templ,GETADDR(RN)-templ);
                                                armregs[RD]=GETADDR(RN)-templ;
                                        }
                                        cycles--;
                                        break;

                                        case 0x26: /*RSB imm*/
                                        if (RD==15)
                                        {
                                                templ=rotate2(opcode);
                                                armregs[15]=(((templ-GETADDR(RN))+4)&0x3FFFFFC)|(armregs[15]&0xFC000003);
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=rotate2(opcode);
                                                armregs[RD]=templ-GETADDR(RN);
                                        }
                                        cycles--;
                                        break;
                                        case 0x27: /*RSBS imm*/
                                        if (RD==15)
                                        {
                                                templ=rotate2(opcode);
                                                armregs[15]=(templ-GETADDR(RN))+4;
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=rotate2(opcode);
                                                setsub(templ,GETADDR(RN),templ-GETADDR(RN));
                                                armregs[RD]=templ-GETADDR(RN);
                                        }
                                        cycles--;
                                        break;

                                        case 0x28: /*ADD imm*/
                                        if (RD==15)
                                        {
                                                templ=rotate2(opcode);
                                                armregs[15]=(((GETADDR(RN)+templ)+4)&0x3FFFFFC)|(armregs[15]&0xFC000003);
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=rotate2(opcode);
                                                armregs[RD]=GETADDR(RN)+templ;
                                        }
                                        cycles--;
                                        break;
                                        case 0x29: /*ADDS imm*/
                                        if (RD==15)
                                        {
                                                templ=rotate2(opcode);
                                                armregs[15]=GETADDR(RN)+templ+4;
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=rotate2(opcode);
                                                setadd(GETADDR(RN),templ,GETADDR(RN)+templ);
                                                armregs[RD]=GETADDR(RN)+templ;
                                        }
                                        cycles--;
                                        break;

                                        case 0x2A: /*ADC imm*/
                                        if (RD==15)
                                        {
                                                templ2=CFSET;
                                                templ=rotate2(opcode);
                                                armregs[15]=((GETADDR(RN)+templ+templ2+4)&0x3FFFFFC)|(armregs[15]&0xFC000003);
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ2=CFSET;
                                                templ=rotate2(opcode);
                                                armregs[RD]=GETADDR(RN)+templ+templ2;
                                        }
                                        cycles--;
                                        break;
                                        case 0x2B: /*ADCS imm*/
                                        if (RD==15)
                                        {
                                                templ2=CFSET;
                                                templ=rotate2(opcode);
                                                armregs[15]=GETADDR(RN)+templ+templ2+4;
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ2=CFSET;
                                                templ=rotate2(opcode);
                                                setadc(GETADDR(RN),templ,GETADDR(RN)+templ+templ2);
                                                armregs[RD]=GETADDR(RN)+templ+templ2;
                                        }
                                        cycles--;
                                        break;

                                        case 0x2C: /*SBC imm*/
                                        templ2=(CFSET)?0:1;
                                        if (RD==15)
                                        {
                                                templ=rotate2(opcode);
                                                armregs[15]=(((GETADDR(RN)-(templ+templ2))+4)&0x3FFFFFC)|(armregs[15]&0xFC000003);
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=rotate2(opcode);
                                                armregs[RD]=GETADDR(RN)-(templ+templ2);
                                        }
                                        cycles--;
                                        break;
                                        case 0x2D: /*SBCS imm*/
                                        templ2=(CFSET)?0:1;
                                        if (RD==15)
                                        {
                                                templ=rotate2(opcode);
                                                armregs[15]=(GETADDR(RN)-(templ+templ2))+4;
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=rotate2(opcode);
                                                setsbc(GETADDR(RN),templ,GETADDR(RN)-(templ+templ2));
                                                armregs[RD]=GETADDR(RN)-(templ+templ2);
                                        }
                                        cycles--;
                                        break;
                                        case 0x2E: /*RSC imm*/
                                        templ2=(CFSET)?0:1;
                                        if (RD==15)
                                        {
                                                templ=rotate2(opcode);
                                                armregs[15]=(((templ-(GETADDR(RN)+templ2))+4)&0x3FFFFFC)|(armregs[15]&0xFC000003);
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=rotate2(opcode);
                                                armregs[RD]=templ-(GETADDR(RN)+templ2);
                                        }
                                        cycles--;
                                        break;
                                        case 0x2F: /*RSCS imm*/
                                        templ2=(CFSET)?0:1;
                                        if (RD==15)
                                        {
                                                templ=rotate2(opcode);
                                                armregs[15]=(templ-(GETADDR(RN)+templ2))+4;
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=rotate2(opcode);
                                                setsbc(templ,GETADDR(RN),templ-(GETADDR(RN)+templ2));
//                                                setsub(templ,GETADDR(RN),templ-(GETADDR(RN)+templ2));
                                                armregs[RD]=templ-(GETADDR(RN)+templ2);
                                        }
                                        cycles--;
                                        break;

                                        case 0x31: /*TST imm*/
                                        if (RD==15)
                                        {
                                                if (armregs[15]&3)
                                                {
                                                        templ=armregs[15]&0x3FFFFFC;
                                                        armregs[15]=((GETADDR(RN)&rotate2(opcode))&0xFC000003)|templ;
                                                }
                                                else
                                                {
                                                        templ=armregs[15]&0x0FFFFFFF;
                                                        armregs[15]=((GETADDR(RN)&rotate2(opcode))&0xF0000000)|templ;
                                                }
                                        }
                                        else
                                        {
                                                setzn(GETADDR(RN)&rotate(opcode));
                                        }
                                        cycles--;
                                        break;
                                
                                        case 0x33: /*TEQ imm*/
                                        if (RD==15)
                                        {
                                                opcode&=~0x100000;
                                                if (armregs[15]&3)
                                                {
                                                        templ=armregs[15]&0x3FFFFFC;
//                                                        if (PC<0x1800000) rpclog("%07X : TEQP - %08X %08X %08X  ",PC,armregs[15],GETADDR(RN),rotate2(opcode));
                                                        armregs[15]=((GETADDR(RN)^rotate2(opcode))&0xFC000003)|templ;
//                                                        if (PC<0x1800000) rpclog(" - %08X\n",armregs[15]);
                                                }
                                                else
                                                {
                                                        templ=armregs[15]&0x0FFFFFFF;
                                                        armregs[15]=((GETADDR(RN)^rotate2(opcode))&0xF0000000)|templ;
                                                }
                                        }
                                        else
                                        {
                                                setzn(GETADDR(RN)^rotate(opcode));
                                        }
                                        cycles--;
                                        break;

                                        case 0x34:
                                        break;
                                        case 0x35: /*CMP imm*/
                                        if (RD==15)
                                        {
                                                if (armregs[15]&3)
                                                {
                                                        templ=armregs[15]&0x3FFFFFC;
                                                        armregs[15]=((GETADDR(RN)-rotate2(opcode))&0xFC000003)|templ;
                                                }
                                                else
                                                {
                                                        templ=armregs[15]&0x0FFFFFFF;
                                                        armregs[15]=((GETADDR(RN)-rotate2(opcode))&0xF0000000)|templ;
                                                }
                                        }
                                        else
                                           setsub(GETADDR(RN),rotate2(opcode),GETADDR(RN)-rotate2(opcode));
                                        cycles--;
                                        break;

                                        case 0x37: /*CMN imm*/
                                        if (RD==15)
                                        {
                                                if (armregs[15]&3)
                                                {
                                                        templ=armregs[15]&0x3FFFFFC;
                                                        armregs[15]=((GETADDR(RN)+rotate2(opcode))&0xFC000003)|templ;
                                                }
                                                else
                                                {
                                                        templ=armregs[15]&0x0FFFFFFF;
                                                        armregs[15]=((GETADDR(RN)+rotate2(opcode))&0xF0000000)|templ;
                                                }
                                        }
                                        else
                                           setadd(GETADDR(RN),rotate2(opcode),GETADDR(RN)+rotate2(opcode));
                                        cycles--;
                                        break;

                                        case 0x38: /*ORR imm*/
                                        if (RD==15)
                                        {
                                                templ=rotate2(opcode);
                                                armregs[15]=(((GETADDR(RN)|templ)+4)&0x3FFFFFC)|(armregs[15]&0xFC000003);
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=rotate2(opcode);
                                                armregs[RD]=GETADDR(RN)|templ;
                                        }
                                        cycles--;
                                        break;
                                        case 0x39: /*ORRS imm*/
                                        if (RD==15)
                                        {
                                                templ=rotate2(opcode);
                                                if (armregs[15]&3)
                                                   armregs[15]=(GETADDR(RN)|templ)+4;
                                                else
                                                   armregs[15]=(((GETADDR(RN)|templ)+4)&0xF3FFFFFC)|(armregs[15]&0xC000003);
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=rotate(opcode);
                                                armregs[RD]=GETADDR(RN)|templ;
                                                setzn(armregs[RD]);
                                        }
                                        cycles--;
                                        break;

                                        case 0x3A: /*MOV imm*/
                                        if (RD==15)
                                        {
                                                armregs[15]=(armregs[15]&0xFC000003)|((rotate2(opcode)+4)&0x3FFFFFC);
                                                refillpipeline();
                                        }
                                        else
                                           armregs[RD]=rotate2(opcode);
                                        cycles--;
                                        break;
                                        case 0x3B: /*MOVS imm*/
                                        if (RD==15)
                                        {
                                                armregs[15]=rotate2(opcode)+4;
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                armregs[RD]=rotate(opcode);
                                                setzn(armregs[RD]);
                                        }
                                        cycles--;
                                        break;

                                        case 0x3C: /*BIC imm*/
                                        if (RD==15)
                                        {
                                                templ=rotate2(opcode);
                                                armregs[15]=(((GETADDR(RN)&~templ)+4)&0x3FFFFFC)|(armregs[15]&0xFC000003);
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=rotate2(opcode);
                                                armregs[RD]=GETADDR(RN)&~templ;
                                        }
                                        cycles--;
                                        break;
                                        case 0x3D: /*BICS imm*/
                                        if (RD==15)
                                        {
                                                templ=rotate2(opcode);
                                                if (armregs[15]&3) armregs[15]=(GETADDR(RN)&~templ)+4;
                                                else               armregs[15]=(((GETADDR(RN)&~templ)+4)&0xF3FFFFFC)|(armregs[15]&0xC000003);
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                templ=rotate(opcode);
                                                armregs[RD]=GETADDR(RN)&~templ;
                                                setzn(armregs[RD]);
                                        }
                                        cycles--;
                                        break;

                                        case 0x3E: /*MVN imm*/
                                        if (RD==15)
                                        {
                                                armregs[15]=(armregs[15]&0xFC000003)|(((~rotate2(opcode))+4)&0x3FFFFFC);
                                                refillpipeline();
                                        }
                                        else
                                           armregs[RD]=~rotate2(opcode);
                                        cycles--;
                                        break;
                                        case 0x3F: /*MVNS imm*/
                                        if (RD==15)
                                        {
                                                armregs[15]=(~rotate2(opcode))+4;
                                                refillpipeline();
                                        }
                                        else
                                        {
                                                armregs[RD]=~rotate(opcode);
                                                setzn(armregs[RD]);
                                        }
                                        cycles--;
                                        break;
/*case 0x40:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
if (addr&0xFC000000) { databort=2; break; }
writememl(addr,armregs[RD]);
if (databort) break;
addr-=addr2;
armregs[RN]=addr;
cycles-=2;
break;

case 0x41:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
if (addr&0xFC000000) { databort=2; break; }
templ=readmeml(addr);
templ=ldrresult(templ,addr);
if (databort) break;
addr-=addr2;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x42:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
templ2=memmode;
memmode=0;
if (addr&0xFC000000) { databort=2; break; }
writememl(addr,armregs[RD]);
memmode=templ2;
if (databort) break;
addr-=addr2;
armregs[RN]=addr;
cycles-=2;
break;

case 0x43:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
templ2=memmode;
memmode=0;
if (addr&0xFC000000) { databort=2; break; }
templ=readmeml(addr);
templ=ldrresult(templ,addr);
memmode=templ2;
if (databort) break;
addr-=addr2;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x44:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
if (addr&0xFC000000) { databort=2; break; }
writememb(addr,armregs[RD]);
if (databort) break;
addr-=addr2;
armregs[RN]=addr;
cycles-=2;
break;

case 0x45:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
if (addr&0xFC000000) { databort=2; break; }
templ=readmemb(addr);
if (databort) break;
addr-=addr2;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x46:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
templ2=memmode;
memmode=0;
if (addr&0xFC000000) { databort=2; break; }
writememb(addr,armregs[RD]);
memmode=templ2;
if (databort) break;
addr-=addr2;
armregs[RN]=addr;
cycles-=2;
break;

case 0x47:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
templ2=memmode;
memmode=0;
if (addr&0xFC000000) { databort=2; break; }
templ=readmemb(addr);
memmode=templ2;
if (databort) break;
addr-=addr2;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x48:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
if (addr&0xFC000000) { databort=2; break; }
writememl(addr,armregs[RD]);
if (databort) break;
addr+=addr2;
armregs[RN]=addr;
cycles-=2;
break;

case 0x49:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
if (addr&0xFC000000) { databort=2; break; }
templ=readmeml(addr);
templ=ldrresult(templ,addr);
if (databort) break;
addr+=addr2;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x4A:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
templ2=memmode;
memmode=0;
if (addr&0xFC000000) { databort=2; break; }
writememl(addr,armregs[RD]);
memmode=templ2;
if (databort) break;
addr+=addr2;
armregs[RN]=addr;
cycles-=2;
break;

case 0x4B:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
templ2=memmode;
memmode=0;
if (addr&0xFC000000) { databort=2; break; }
templ=readmeml(addr);
templ=ldrresult(templ,addr);
memmode=templ2;
if (databort) break;
addr+=addr2;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x4C:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
if (addr&0xFC000000) { databort=2; break; }
writememb(addr,armregs[RD]);
if (databort) break;
addr+=addr2;
armregs[RN]=addr;
cycles-=2;
break;

case 0x4D:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
if (addr&0xFC000000) { databort=2; break; }
templ=readmemb(addr);
if (databort) break;
addr+=addr2;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x4E:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
templ2=memmode;
memmode=0;
if (addr&0xFC000000) { databort=2; break; }
writememb(addr,armregs[RD]);
memmode=templ2;
if (databort) break;
addr+=addr2;
armregs[RN]=addr;
cycles-=2;
break;

case 0x4F:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
templ2=memmode;
memmode=0;
if (addr&0xFC000000) { databort=2; break; }
templ=readmemb(addr);
memmode=templ2;
if (databort) break;
addr+=addr2;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x50:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
addr-=addr2;
if (addr&0xFC000000) { databort=2; break; }
writememl(addr,armregs[RD]);
if (databort) break;
cycles-=2;
break;

case 0x51:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
addr-=addr2;
if (addr&0xFC000000) { databort=2; break; }
templ=readmeml(addr);
templ=ldrresult(templ,addr);
if (databort) break;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x52:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
addr-=addr2;
if (addr&0xFC000000) { databort=2; break; }
writememl(addr,armregs[RD]);
if (databort) break;
armregs[RN]=addr;
cycles-=2;
break;

case 0x53:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
addr-=addr2;
if (addr&0xFC000000) { databort=2; break; }
templ=readmeml(addr);
templ=ldrresult(templ,addr);
if (databort) break;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x54:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
addr-=addr2;
if (addr&0xFC000000) { databort=2; break; }
writememb(addr,armregs[RD]);
if (databort) break;
cycles-=2;
break;

case 0x55:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
addr-=addr2;
if (addr&0xFC000000) { databort=2; break; }
templ=readmemb(addr);
if (databort) break;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x56:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
addr-=addr2;
if (addr&0xFC000000) { databort=2; break; }
writememb(addr,armregs[RD]);
if (databort) break;
armregs[RN]=addr;
cycles-=2;
break;

case 0x57:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
addr-=addr2;
if (addr&0xFC000000) { databort=2; break; }
templ=readmemb(addr);
if (databort) break;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x58:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
addr+=addr2;
if (addr&0xFC000000) { databort=2; break; }
writememl(addr,armregs[RD]);
if (databort) break;
cycles-=2;
break;

case 0x59:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
addr+=addr2;
if (addr&0xFC000000) { databort=2; break; }
templ=readmeml(addr);
templ=ldrresult(templ,addr);
if (databort) break;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x5A:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
addr+=addr2;
if (addr&0xFC000000) { databort=2; break; }
writememl(addr,armregs[RD]);
if (databort) break;
armregs[RN]=addr;
cycles-=2;
break;

case 0x5B:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
addr+=addr2;
if (addr&0xFC000000) { databort=2; break; }
templ=readmeml(addr);
templ=ldrresult(templ,addr);
if (databort) break;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x5C:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
addr+=addr2;
if (addr&0xFC000000) { databort=2; break; }
writememb(addr,armregs[RD]);
if (databort) break;
cycles-=2;
break;

case 0x5D:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
addr+=addr2;
if (addr&0xFC000000) { databort=2; break; }
templ=readmemb(addr);
if (databort) break;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x5E:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
addr+=addr2;
if (addr&0xFC000000) { databort=2; break; }
writememb(addr,armregs[RD]);
if (databort) break;
armregs[RN]=addr;
cycles-=2;
break;

case 0x5F:
addr=GETADDR(RN);
addr2=opcode&0xFFF;
addr+=addr2;
if (addr&0xFC000000) { databort=2; break; }
templ=readmemb(addr);
if (databort) break;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x60:
addr=GETADDR(RN);
addr2=shift2(opcode);
if (addr&0xFC000000) { databort=2; break; }
writememl(addr,armregs[RD]);
if (databort) break;
addr-=addr2;
armregs[RN]=addr;
cycles-=2;
break;

case 0x61:
addr=GETADDR(RN);
addr2=shift2(opcode);
if (addr&0xFC000000) { databort=2; break; }
templ=readmeml(addr);
templ=ldrresult(templ,addr);
if (databort) break;
addr-=addr2;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x62:
addr=GETADDR(RN);
addr2=shift2(opcode);
templ2=memmode;
memmode=0;
if (addr&0xFC000000) { databort=2; break; }
writememl(addr,armregs[RD]);
memmode=templ2;
if (databort) break;
addr-=addr2;
armregs[RN]=addr;
cycles-=2;
break;

case 0x63:
addr=GETADDR(RN);
addr2=shift2(opcode);
templ2=memmode;
memmode=0;
if (addr&0xFC000000) { databort=2; break; }
templ=readmeml(addr);
templ=ldrresult(templ,addr);
memmode=templ2;
if (databort) break;
addr-=addr2;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x64:
addr=GETADDR(RN);
addr2=shift2(opcode);
if (addr&0xFC000000) { databort=2; break; }
writememb(addr,armregs[RD]);
if (databort) break;
addr-=addr2;
armregs[RN]=addr;
cycles-=2;
break;

case 0x65:
addr=GETADDR(RN);
addr2=shift2(opcode);
if (addr&0xFC000000) { databort=2; break; }
templ=readmemb(addr);
if (databort) break;
addr-=addr2;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x66:
addr=GETADDR(RN);
addr2=shift2(opcode);
templ2=memmode;
memmode=0;
if (addr&0xFC000000) { databort=2; break; }
writememb(addr,armregs[RD]);
memmode=templ2;
if (databort) break;
addr-=addr2;
armregs[RN]=addr;
cycles-=2;
break;

case 0x67:
addr=GETADDR(RN);
addr2=shift2(opcode);
templ2=memmode;
memmode=0;
if (addr&0xFC000000) { databort=2; break; }
templ=readmemb(addr);
memmode=templ2;
if (databort) break;
addr-=addr2;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x68:
addr=GETADDR(RN);
addr2=shift2(opcode);
if (addr&0xFC000000) { databort=2; break; }
writememl(addr,armregs[RD]);
if (databort) break;
addr+=addr2;
armregs[RN]=addr;
cycles-=2;
break;

case 0x69:
addr=GETADDR(RN);
addr2=shift2(opcode);
if (addr&0xFC000000) { databort=2; break; }
templ=readmeml(addr);
templ=ldrresult(templ,addr);
if (databort) break;
addr+=addr2;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x6A:
addr=GETADDR(RN);
addr2=shift2(opcode);
templ2=memmode;
memmode=0;
if (addr&0xFC000000) { databort=2; break; }
writememl(addr,armregs[RD]);
memmode=templ2;
if (databort) break;
addr+=addr2;
armregs[RN]=addr;
cycles-=2;
break;

case 0x6B:
addr=GETADDR(RN);
addr2=shift2(opcode);
templ2=memmode;
memmode=0;
if (addr&0xFC000000) { databort=2; break; }
templ=readmeml(addr);
templ=ldrresult(templ,addr);
memmode=templ2;
if (databort) break;
addr+=addr2;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x6C:
addr=GETADDR(RN);
addr2=shift2(opcode);
if (addr&0xFC000000) { databort=2; break; }
writememb(addr,armregs[RD]);
if (databort) break;
addr+=addr2;
armregs[RN]=addr;
cycles-=2;
break;

case 0x6D:
addr=GETADDR(RN);
addr2=shift2(opcode);
if (addr&0xFC000000) { databort=2; break; }
templ=readmemb(addr);
if (databort) break;
addr+=addr2;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x6E:
addr=GETADDR(RN);
addr2=shift2(opcode);
templ2=memmode;
memmode=0;
if (addr&0xFC000000) { databort=2; break; }
writememb(addr,armregs[RD]);
memmode=templ2;
if (databort) break;
addr+=addr2;
armregs[RN]=addr;
cycles-=2;
break;

case 0x6F:
addr=GETADDR(RN);
addr2=shift2(opcode);
templ2=memmode;
memmode=0;
if (addr&0xFC000000) { databort=2; break; }
templ=readmemb(addr);
memmode=templ2;
if (databort) break;
addr+=addr2;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x70:
addr=GETADDR(RN);
addr2=shift2(opcode);
addr-=addr2;
if (addr&0xFC000000) { databort=2; break; }
writememl(addr,armregs[RD]);
if (databort) break;
cycles-=2;
break;

case 0x71:
addr=GETADDR(RN);
addr2=shift2(opcode);
addr-=addr2;
if (addr&0xFC000000) { databort=2; break; }
templ=readmeml(addr);
templ=ldrresult(templ,addr);
if (databort) break;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x72:
addr=GETADDR(RN);
addr2=shift2(opcode);
addr-=addr2;
if (addr&0xFC000000) { databort=2; break; }
writememl(addr,armregs[RD]);
if (databort) break;
armregs[RN]=addr;
cycles-=2;
break;

case 0x73:
addr=GETADDR(RN);
addr2=shift2(opcode);
addr-=addr2;
if (addr&0xFC000000) { databort=2; break; }
templ=readmeml(addr);
templ=ldrresult(templ,addr);
if (databort) break;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x74:
addr=GETADDR(RN);
addr2=shift2(opcode);
addr-=addr2;
if (addr&0xFC000000) { databort=2; break; }
writememb(addr,armregs[RD]);
if (databort) break;
cycles-=2;
break;

case 0x75:
addr=GETADDR(RN);
addr2=shift2(opcode);
addr-=addr2;
if (addr&0xFC000000) { databort=2; break; }
templ=readmemb(addr);
if (databort) break;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x76:
addr=GETADDR(RN);
addr2=shift2(opcode);
addr-=addr2;
if (addr&0xFC000000) { databort=2; break; }
writememb(addr,armregs[RD]);
if (databort) break;
armregs[RN]=addr;
cycles-=2;
break;

case 0x77:
addr=GETADDR(RN);
addr2=shift2(opcode);
addr-=addr2;
if (addr&0xFC000000) { databort=2; break; }
templ=readmemb(addr);
if (databort) break;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x78:
addr=GETADDR(RN);
addr2=shift2(opcode);
addr+=addr2;
if (addr&0xFC000000) { databort=2; break; }
writememl(addr,armregs[RD]);
if (databort) break;
cycles-=2;
break;

case 0x79:
addr=GETADDR(RN);
addr2=shift2(opcode);
addr+=addr2;
if (addr&0xFC000000) { databort=2; break; }
templ=readmeml(addr);
templ=ldrresult(templ,addr);
if (databort) break;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x7A:
addr=GETADDR(RN);
addr2=shift2(opcode);
addr+=addr2;
if (addr&0xFC000000) { databort=2; break; }
writememl(addr,armregs[RD]);
if (databort) break;
armregs[RN]=addr;
cycles-=2;
break;

case 0x7B:
addr=GETADDR(RN);
addr2=shift2(opcode);
addr+=addr2;
if (addr&0xFC000000) { databort=2; break; }
templ=readmeml(addr);
templ=ldrresult(templ,addr);
if (databort) break;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x7C:
addr=GETADDR(RN);
addr2=shift2(opcode);
addr+=addr2;
if (addr&0xFC000000) { databort=2; break; }
writememb(addr,armregs[RD]);
if (databort) break;
cycles-=2;
break;

case 0x7D:
addr=GETADDR(RN);
addr2=shift2(opcode);
addr+=addr2;
if (addr&0xFC000000) { databort=2; break; }
templ=readmemb(addr);
if (databort) break;
LOADREG(RD,templ);
cycles-=2;
break;

case 0x7E:
addr=GETADDR(RN);
addr2=shift2(opcode);
addr+=addr2;
if (addr&0xFC000000) { databort=2; break; }
writememb(addr,armregs[RD]);
if (databort) break;
armregs[RN]=addr;
cycles-=2;
break;

case 0x7F:
addr=GETADDR(RN);
addr2=shift2(opcode);
addr+=addr2;
if (addr&0xFC000000) { databort=2; break; }
templ=readmemb(addr);
if (databort) break;
armregs[RN]=addr;
LOADREG(RD,templ);
cycles-=2;
break;
*/

                                        case 0x47: case 0x4F: /*LDRBT*/
                                        addr=GETADDR(RN);
                                        if (opcode&0x2000000) addr2=shiftmem(opcode);
                                        else                  addr2=opcode&0xFFF;
                                        if (!(opcode&0x800000))  addr2=-addr2;
                                        if (opcode&0x1000000)
                                        {
                                                addr+=addr2;
                                        }
                                        templ=memmode;
                                        memmode=0;
                                        templ2=readmemb(addr);
                                        memmode=templ;
                                        if (databort) break;
                                        LOADREG(RD,templ2);
                                        if (!(opcode&0x1000000))
                                        {
                                                addr+=addr2;
                                                armregs[RN]=addr;
                                        }
                                        else
                                        {
                                                if (opcode&0x200000) armregs[RN]=addr;
                                        }
                                        cycles-=4;
/*                                        if (RD==7)
                                        {
                                                if (!olog) olog=fopen("armlog.txt","wt");
                                                sprintf(s,"LDRB R7 %02X,%07X\n",armregs[7],PC);
                                                fputs(s,olog);
                                        }*/
                                        break;

                                        case 0x41: case 0x49: case 0x61: case 0x69: /*LDR Rd,[Rn],offset*/
                                        addr=GETADDR(RN);
                                        if (opcode&0x2000000) addr2=shiftmem(opcode);
                                        else                  addr2=opcode&0xFFF;
                                        templ2=ldrresult(readmeml(addr),addr);
                                        if (databort) break;
                                        LOADREG(RD,templ2);
                                        if (opcode&0x800000) armregs[RN]+=addr2;
                                        else                 armregs[RN]-=addr2;
                                        cycles-=4;
                                        break;
                                        case 0x43: case 0x4B: case 0x63: case 0x6B: /*LDRT Rd,[Rn],offset*/
                                        addr=GETADDR(RN);
                                        if (opcode&0x2000000) addr2=shiftmem(opcode);
                                        else                  addr2=opcode&0xFFF;
                                        templ=memmode; memmode=0;
                                        templ2=ldrresult(readmeml(addr),addr);
                                        memmode=templ;
                                        if (databort) break;
                                        LOADREG(RD,templ2);
                                        if (opcode&0x800000) armregs[RN]+=addr2;
                                        else                 armregs[RN]-=addr2;
                                        cycles-=4;
                                        break;

                                        case 0x40: case 0x48: case 0x60: case 0x68: /*STR Rd,[Rn],offset*/
                                        addr=GETADDR(RN);
                                        if (opcode&0x2000000) addr2=shiftmem(opcode);
                                        else                  addr2=opcode&0xFFF;
                                        if (RD==15) { writememl(addr,armregs[RD]+4); }
                                        else        { writememl(addr,armregs[RD]); }
                                        if (databort) break;
                                        if (opcode&0x800000) armregs[RN]+=addr2;
                                        else                 armregs[RN]-=addr2;
                                        cycles-=3;
                                        break;
                                        case 0x42: case 0x4A: case 0x62: case 0x6A: /*STRT Rd,[Rn],offset*/
                                        addr=GETADDR(RN);
                                        if (opcode&0x2000000) addr2=shiftmem(opcode);
                                        else                  addr2=opcode&0xFFF;
                                        if (RD==15) { writememl(addr,armregs[RD]+4); }
                                        else        { writememl(addr,armregs[RD]); }
                                        templ=memmode; memmode=0;
                                        if (databort) break;
                                        memmode=templ;
                                        if (opcode&0x800000) armregs[RN]+=addr2;
                                        else                 armregs[RN]-=addr2;
                                        cycles-=3;
                                        break;
                                        case 0x50: case 0x58: case 0x70: case 0x78: /*STR Rd,[Rn,offset]*/
                                        case 0x52: case 0x5A: case 0x72: case 0x7A: /*STR Rd,[Rn,offset]!*/
                                        if (opcode&0x2000000) addr2=shiftmem(opcode);
                                        else                  addr2=opcode&0xFFF;
                                        if (opcode&0x800000) addr=GETADDR(RN)+addr2;
                                        else                 addr=GETADDR(RN)-addr2;
                                        if (RD==15) { writememl(addr,armregs[RD]+4); }
                                        else        { writememl(addr,armregs[RD]); }
                                        if (databort) break;
                                        if (opcode&0x200000) armregs[RN]=addr;
                                        cycles-=3;
                                        break;

                                        case 0x44: case 0x4C: case 0x64: case 0x6C: /*STRB Rd,[Rn],offset*/
                                        addr=GETADDR(RN);
                                        if (opcode&0x2000000) addr2=shiftmem(opcode);
                                        else                  addr2=opcode&0xFFF;
                                        writememb(addr,armregs[RD]);
                                        if (databort) break;
                                        if (opcode&0x800000) armregs[RN]+=addr2;
                                        else                 armregs[RN]-=addr2;
                                        cycles-=3;
                                        break;
                                        case 0x46: case 0x4E: case 0x66: case 0x6E: /*STRBT Rd,[Rn],offset*/
                                        addr=GETADDR(RN);
                                        if (opcode&0x2000000) addr2=shiftmem(opcode);
                                        else                  addr2=opcode&0xFFF;
                                        writememb(addr,armregs[RD]);
                                        templ=memmode;
                                        memmode=0;
                                        if (databort) break;
                                        memmode=templ;
                                        if (opcode&0x800000) armregs[RN]+=addr2;
                                        else                 armregs[RN]-=addr2;
                                        cycles-=3;
                                        break;
                                        case 0x54: case 0x5C: case 0x74: case 0x7C: /*STRB Rd,[Rn,offset]*/
                                        case 0x56: case 0x5E: case 0x76: case 0x7E: /*STRB Rd,[Rn,offset]!*/
                                        if (opcode&0x2000000) addr2=shiftmem(opcode);
                                        else                  addr2=opcode&0xFFF;
                                        if (opcode&0x800000) addr=GETADDR(RN)+addr2;
                                        else                 addr=GETADDR(RN)-addr2;
                                        writememb(addr,armregs[RD]);
                                        if (databort) break;
                                        if (opcode&0x200000) armregs[RN]=addr;
                                        cycles-=3;
                                        break;


//                                        case 0x41: case 0x49: /*LDR*/
                                        case 0x51: case 0x53: case 0x59: case 0x5B:
//                                        case 0x61: case 0x69:
                                        case 0x71: case 0x73: case 0x79: case 0x7B:
                                        if ((opcode&0x2000010)==0x2000010)
                                        {
                                                undefined();
                                                break;
                                        }
                                        addr=GETADDR(RN);
                                        if (opcode&0x2000000) addr2=shiftmem(opcode);
                                        else                  addr2=opcode&0xFFF;
                                        if (!(opcode&0x800000))  addr2=-addr2;
                                        if (opcode&0x1000000)
                                        {
                                                addr+=addr2;
                                        }
                                        templ=readmeml(addr);
                                        templ=ldrresult(templ,addr);
                                        if (databort) break;
                                        if (!(opcode&0x1000000))
                                        {
                                                addr+=addr2;
                                                armregs[RN]=addr;
                                        }
                                        else
                                        {
                                                if (opcode&0x200000) armregs[RN]=addr;
                                        }
                                        LOADREG(RD,templ);
//                                        if (RD==15) refillpipeline();
                                        cycles-=4;
/*                                        if (RD==7)
                                        {
                                                if (!olog) olog=fopen("armlog.txt","wt");
                                                sprintf(s,"LDR R7 %08X,%07X\n",armregs[7],PC);
                                                fputs(s,olog);
                                        }*/
                                        break;

                                        case 0x65: case 0x6D:
                                        case 0x75: case 0x77: case 0x7D: case 0x7F:
                                        if (opcode&0x10)
                                        {
                                                undefined();
                                                break;
                                        }
                                        case 0x45: case 0x4D: /*LDRB*/
                                        case 0x55: case 0x57: case 0x5D: case 0x5F:
                                        addr=GETADDR(RN);
                                        if (opcode&0x2000000) addr2=shiftmem(opcode);
                                        else                  addr2=opcode&0xFFF;
                                        if (!(opcode&0x800000))  addr2=-addr2;
                                        if (opcode&0x1000000)
                                        {
                                                addr+=addr2;
                                        }
                                        templ=readmemb(addr);
                                        if (databort) break;
                                        if (!(opcode&0x1000000))
                                        {
                                                addr+=addr2;
                                                armregs[RN]=addr;
                                        }
                                        else
                                        {
                                                if (opcode&0x200000) armregs[RN]=addr;
                                        }
                                        armregs[RD]=templ;
                                        cycles-=4;
                                        break;

#define STMfirst()      mask=1; \
                        for (c=0;c<15;c++) \
                        { \
                                if (opcode&mask) \
                                { \
                                        if (!(addr&0xC)) cycles--; \
                                        if (c==15) { writememl(addr,armregs[c]+4); } \
                                        else       { writememl(addr,armregs[c]); } \
                                        addr+=4; \
                                        cycles--; \
                                        break; \
                                } \
                                mask<<=1; \
                        } \
                        mask<<=1; c++;
                        
#define STMall()        for (;c<15;c++) \
                        { \
                                if (opcode&mask) \
                                { \
                                        if (!(addr&0xC)) cycles--; \
                                        writememl(addr,armregs[c]); \
                                        addr+=4; \
                                        cycles--; \
                                } \
                                mask<<=1; \
                        } \
                        if (opcode&0x8000) \
                        { \
                                if (!(addr&0xC)) cycles--; \
                                writememl(addr,armregs[15]+4); \
                                cycles--; \
                        }

#define STMfirstS()     mask=1; \
                        for (c=0;c<15;c++) \
                        { \
                                if (opcode&mask) \
                                { \
                                        if (!(addr&0xC)) cycles--; \
                                        if (c==15) { writememl(addr,armregs[c]+4); } \
                                        else       { writememl(addr,*usrregs[c]); } \
                                        addr+=4; \
                                        cycles--; \
                                        break; \
                                } \
                                mask<<=1; \
                        } \
                        mask<<=1; c++;

#define STMallS()       for (;c<15;c++) \
                        { \
                                if (opcode&mask) \
                                { \
                                        if (!(addr&0xC)) cycles--; \
                                        writememl(addr,*usrregs[c]); \
                                        addr+=4; \
                                        cycles--; \
                                } \
                                mask<<=1; \
                        } \
                        if (opcode&0x8000) \
                        { \
                                if (!(addr&0xC)) cycles--; \
                                writememl(addr,armregs[15]+4); \
                                cycles--; \
                        }

#define LDMall()        mask=1; \
                        for (c=0;c<15;c++) \
                        { \
                                if (opcode&mask) \
                                { \
                                        if (!(addr&0xC)) cycles--; \
                                        templ=readmeml(addr); if (!databort) armregs[c]=templ; \
                                        addr+=4; \
                                        cycles--; \
                                } \
                                mask<<=1; \
                        } \
                        if (opcode&0x8000) \
                        { \
                                if (!(addr&0xC)) cycles--; \
                                templ=readmeml(addr); \
                                if (!databort) armregs[15]=(armregs[15]&0xFC000003)|((templ+4)&0x3FFFFFC); \
                                cycles--; \
                                refillpipeline(); \
                        }

#define LDMallS()       mask=1; \
                        if (opcode&0x8000) \
                        { \
                                for (c=0;c<15;c++) \
                                { \
                                        if (opcode&mask) \
                                        { \
                                                if (!(addr&0xC)) cycles--; \
                                                templ=readmeml(addr); if (!databort) armregs[c]=templ; \
                                                addr+=4; \
                                                cycles--; \
                                        } \
                                        mask<<=1; \
                                } \
                                if (!(addr&0xC)) cycles--; \
                                templ=readmeml(addr); \
                                if (!databort) \
                                { \
                                if (output) rpclog("Loading R15 with %08X\n",templ); \
                                        if (armregs[15]&3) armregs[15]=(templ+4); \
                                        else               armregs[15]=(armregs[15]&0x0C000003)|((templ+4)&0xF3FFFFFC); \
                                } \
                                cycles--; \
                                refillpipeline(); \
                        } \
                        else \
                        { \
                                for (c=0;c<15;c++) \
                                { \
                                        if (opcode&mask) \
                                        { \
                                                if (!(addr&0xC)) cycles--; \
                                                templ=readmeml(addr); if (!databort) *usrregs[c]=templ; \
                                                addr+=4; \
                                                cycles--; \
                                        } \
                                        mask<<=1; \
                                } \
                        }

                                        case 0x80: /*STMDA*/
                                        case 0x82: /*STMDA !*/
                                        case 0x90: /*STMDB*/
                                        case 0x92: /*STMDB !*/
                                        addr=armregs[RN]-countbits(opcode&0xFFFF);
                                        if (!(opcode&0x1000000)) addr+=4;
                                        STMfirst();
                                        if (opcode&0x200000) armregs[RN]-=countbits(opcode&0xFFFF);
                                        STMall()
                                        cycles--;
                                        break;
                                        case 0x88: /*STMIA*/
                                        case 0x8A: /*STMIA !*/
                                        case 0x98: /*STMIB*/
                                        case 0x9A: /*STMIB !*/
                                        addr=armregs[RN];
                                        if (opcode&0x1000000) addr+=4;
                                        STMfirst();
                                        if (opcode&0x200000) armregs[RN]+=countbits(opcode&0xFFFF);
                                        STMall();
                                        cycles--;
                                        break;
                                        case 0x84: /*STMDA ^*/
                                        case 0x86: /*STMDA ^!*/
                                        case 0x94: /*STMDB ^*/
                                        case 0x96: /*STMDB ^!*/
                                        addr=armregs[RN]-countbits(opcode&0xFFFF);
                                        if (!(opcode&0x1000000)) addr+=4;
                                        STMfirstS();
                                        if (opcode&0x200000) armregs[RN]-=countbits(opcode&0xFFFF);
                                        STMallS()
                                        cycles--;
                                        break;
                                        case 0x8C: /*STMIA ^*/
                                        case 0x8E: /*STMIA ^!*/
                                        case 0x9C: /*STMIB ^*/
                                        case 0x9E: /*STMIB ^!*/
                                        addr=armregs[RN];
                                        if (opcode&0x1000000) addr+=4;
                                        STMfirstS();
                                        if (opcode&0x200000) armregs[RN]+=countbits(opcode&0xFFFF);
                                        STMallS();
                                        cycles--;
                                        break;
                                        
                                        case 0x81: /*LDMDA*/
                                        case 0x83: /*LDMDA !*/
                                        case 0x91: /*LDMDB*/
                                        case 0x93: /*LDMDB !*/
                                        addr=armregs[RN]-countbits(opcode&0xFFFF);
//                                        rpclog("LDMDB %08X\n",addr);
                                        if (!(opcode&0x1000000)) addr+=4;
                                        if (opcode&0x200000) armregs[RN]-=countbits(opcode&0xFFFF);
                                        LDMall();
                                        cycles-=2;
                                        break;
                                        case 0x89: /*LDMIA*/
                                        case 0x8B: /*LDMIA !*/
                                        case 0x99: /*LDMIB*/
                                        case 0x9B: /*LDMIB !*/
                                        addr=armregs[RN];
                                        if (opcode&0x1000000) addr+=4;
                                        if (opcode&0x200000) armregs[RN]+=countbits(opcode&0xFFFF);
                                        LDMall();
                                        cycles-=2;
                                        break;
                                        case 0x85: /*LDMDA ^*/
                                        case 0x87: /*LDMDA ^!*/
                                        case 0x95: /*LDMDB ^*/
                                        case 0x97: /*LDMDB ^!*/
                                        addr=armregs[RN]-countbits(opcode&0xFFFF);
                                        if (!(opcode&0x1000000)) addr+=4;
                                        if (opcode&0x200000) armregs[RN]-=countbits(opcode&0xFFFF);
                                        LDMallS();
                                        cycles-=2;
                                        break;
                                        case 0x8D: /*LDMIA ^*/
                                        case 0x8F: /*LDMIA ^!*/
                                        case 0x9D: /*LDMIB ^*/
                                        case 0x9F: /*LDMIB ^!*/
                                        addr=armregs[RN];
                                        if (opcode&0x1000000) addr+=4;
                                        if (opcode&0x200000) armregs[RN]+=countbits(opcode&0xFFFF);
                                        LDMallS();
                                        cycles-=2;
                                        break;

                                        case 0xB0: case 0xB1: case 0xB2: case 0xB3: /*BL*/
                                        case 0xB4: case 0xB5: case 0xB6: case 0xB7:
                                        case 0xB8: case 0xB9: case 0xBA: case 0xBB:
                                        case 0xBC: case 0xBD: case 0xBE: case 0xBF:
                                        templ=(opcode&0xFFFFFF)<<2;
                                        armregs[14]=armregs[15]-4;
                                        armregs[15]=((armregs[15]+templ+4)&0x3FFFFFC)|(armregs[15]&0xFC000003);
                                        refillpipeline();
                                        cycles-=4;
                                        break;

                                        case 0xA0: case 0xA1: case 0xA2: case 0xA3: /*B*/
                                        case 0xA4: case 0xA5: case 0xA6: case 0xA7:
                                        case 0xA8: case 0xA9: case 0xAA: case 0xAB:
                                        case 0xAC: case 0xAD: case 0xAE: case 0xAF:
                                        templ=(opcode&0xFFFFFF)<<2;
                                        armregs[15]=((armregs[15]+templ+4)&0x3FFFFFC)|(armregs[15]&0xFC000003);
                                        refillpipeline();
                                        cycles-=4;
                                        break;

                                        case 0xE0: case 0xE2: case 0xE4: case 0xE6: /*MCR*/
                                        case 0xE8: case 0xEA: case 0xEC: case 0xEE:
                                        if (fpaena && MULRS==1)
                                        {
                                                if (fpaopcode(opcode))
                                                {
                                                        templ=armregs[15]-4;
                                                        armregs[15]|=3;
                                                        updatemode(SUPERVISOR);
                                                        armregs[14]=templ;
                                                        armregs[15]&=0xFC000003;
                                                        armregs[15]|=0x08000008;
                                                        cycles-=4;
                                                        refillpipeline();
                                                }
                                        }
                                        else if (MULRS==15 && (opcode&0x10) && (arm3&1))
                                        {
                                                writecp15(RN,armregs[RD]);
                                        }
                                        else
                                        {
                                                templ=armregs[15]-4;
                                                armregs[15]|=3;
                                                updatemode(SUPERVISOR);
                                                armregs[14]=templ;
                                                armregs[15]&=0xFC000003;
                                                armregs[15]|=0x08000008;
                                                cycles-=4;
                                                refillpipeline();
                                        }
                                        break;

                                        case 0xE1: case 0xE3: case 0xE5: case 0xE7: /*MRC*/
                                        case 0xE9: case 0xEB: case 0xED: case 0xEF:
                                        if (fpaena && MULRS==1)
                                        {
                                                if (fpaopcode(opcode))
                                                {
                                                        templ=armregs[15]-4;
                                                        armregs[15]|=3;
                                                        updatemode(SUPERVISOR);
                                                        armregs[14]=templ;
                                                        armregs[15]&=0xFC000003;
                                                        armregs[15]|=0x08000008;
                                                        cycles-=4;
                                                        refillpipeline();
                                                }
                                        }
                                        else if (MULRS==15 && (opcode&0x10) && (arm3&1))
                                        {
                                                if (RD==15) armregs[RD]=(armregs[RD]&0x3FFFFFC)|(readcp15(RN)&0xFC000003);
                                                else        armregs[RD]=readcp15(RN);
                                        }
                                        else
                                        {
                                                templ=armregs[15]-4;
                                                armregs[15]|=3;
                                                updatemode(SUPERVISOR);
                                                armregs[14]=templ;
                                                armregs[15]&=0xFC000003;
                                                armregs[15]|=0x08000008;
                                                cycles-=4;
                                                refillpipeline();
                                        }
                                        break;
                                        
                                        case 0xC0: case 0xC1: case 0xC2: case 0xC3: /*Co-pro*/
                                        case 0xC4: case 0xC5: case 0xC6: case 0xC7:
                                        case 0xC8: case 0xC9: case 0xCA: case 0xCB:
                                        case 0xCC: case 0xCD: case 0xCE: case 0xCF:
                                        case 0xD0: case 0xD1: case 0xD2: case 0xD3:
                                        case 0xD4: case 0xD5: case 0xD6: case 0xD7:
                                        case 0xD8: case 0xD9: case 0xDA: case 0xDB:
                                        case 0xDC: case 0xDD: case 0xDE: case 0xDF:
                                        if ((opcode>=0xEC500000) && (opcode<=0xEC500008))
                                           arculfs(opcode&15);
                                        else if (((opcode&0xF00)==0x100 || (opcode&0xF00)==0x200) && fpaena)
                                        {
                                                if (fpaopcode(opcode))
                                                {
                                                        templ=armregs[15]-4;
                                                        armregs[15]|=3;
                                                        updatemode(SUPERVISOR);
                                                        armregs[14]=templ;
                                                        armregs[15]&=0xFC000003;
                                                        armregs[15]|=0x08000008;
                                                        cycles-=4;
                                                        refillpipeline();
                                                }
                                        }
                                        else
                                        {
                                                templ=armregs[15]-4;
                                                armregs[15]|=3;
                                                updatemode(SUPERVISOR);
                                                armregs[14]=templ;
                                                armregs[15]&=0xFC000003;
                                                armregs[15]|=0x08000008;
                                                cycles-=4;
                                                refillpipeline();
                                        }
                                        break;

                                        case 0xF0: case 0xF1: case 0xF2: case 0xF3: /*SWI*/
                                        case 0xF4: case 0xF5: case 0xF6: case 0xF7:
                                        case 0xF8: case 0xF9: case 0xFA: case 0xFB:
                                        case 0xFC: case 0xFD: case 0xFE: case 0xFF:
/*                                        if ((opcode&0x4FFFF)==0x400C2)
                                        {
                                                rpclog("Wimp_CreateIcon %07X %07X %08X handle %08X %08X %08X %08X %08X\n",PC,armregs[1],armregs[0],readmeml(armregs[1]),readmeml(armregs[1]+4),readmeml(armregs[1]+8),readmeml(armregs[1]+12),readmeml(armregs[1]+16));
                                        }*/
/*                                                if (output) rpclog("SWI %08X %07X  %08X %08X %08X %08X %08X\n",opcode&0x4FFFF,PC,armregs[0],armregs[1],armregs[2],armregs[3],armregs[4]);*/
                                        /*if ((opcode&0x4FFFF)==0x40240)
                                        {
                                                rpclog("SWI ADFS_DiscOp %05X %08X %08X %08X %08X %08X %07X\n",opcode&0xFFFFF,armregs[0],armregs[1],armregs[2],armregs[3],armregs[4],PC);
//                                                if (PC==0x24460) output=1;
                                        }
                                        if ((opcode&0x4FFFF)==0x40540)
                                        {
                                                rpclog("SWI FileCore_DiscOp %05X %08X %08X %08X %08X %08X %07X\n",opcode&0xFFFFF,armregs[0],armregs[1],armregs[2],armregs[3],armregs[4],PC);
                                        }*/
//                                                if ((opcode&0xFFF)==0x2B) output=0;
//                                                if (output==1) output=2;
/*                                        if (PC<0x1800000)
                                        {*/
                                        
                                        if ((opcode&0x4FFFF) == 0x40240)
                                           rpclog("ADFS_DiscOp %08X %08X %08X %08X  %07X\n",armregs[1],armregs[2],armregs[3],armregs[4],PC);
                                        if ((opcode&0x4FFFF) == 0x40540)
                                           rpclog("FileCore_DiscOp %08X %08X %08X %08X  %07X\n",armregs[1],armregs[2],armregs[3],armregs[4],PC);

/*                                        if (swioutput)
                                        {
                                                rpclog("SWI %05X %08X %08X %08X %08X %c %c %07X %i\n",opcode&0xFFFFF,armregs[0],armregs[1],armregs[2],armregs[3],((opcode&0xFF)>0x20)?(opcode&0xFF):'.',((armregs[0]&0xFF)>0x20)?(armregs[0]&0xFF):'.',PC,inssinceswi);
                                                inssinceswi=0;
                                        }*/
/*                                                if ((opcode&0x1FFFF)==0x11)
                                                {
                                                        rpclog("OS_Exit!\n");
                                                        dumpregs();
                                                        exit(-1);
                                                }*/
                                                

/*                                                if (PC==0x19E60) output=1;
//                                                if (PC==0x19E60) output=1;
//                                                if (PC==0x9A88) output=0;
                                                lastswi=PC;
                                        }
                                        if ((opcode&0xFFFF)==0x11) output=0;*/
                                                templ=armregs[15]-4;
                                                armregs[15]|=3;
                                                updatemode(SUPERVISOR);
                                                armregs[14]=templ;
                                                armregs[15]&=0xFC000003;
//                                                armregs[15]|=0x0800000C;
                                                armregs[15]|=0x0800000C;
                                                cycles-=4;
                                                refillpipeline();
                                        break;

                                        default:
                                        sprintf(s,"Bad opcode %02X %08X at %07X\n",(opcode>>20)&0xFF,opcode,PC);
                                        MessageBox(NULL,s,"Arc",MB_OK);
                                        dumpregs();
                                        exit(-1);
                                }
                        }
                        else if (!prefabort)
                           cycles--;
                        if (databort|armirq|prefabort)
                        {
                                if (prefabort)       /*Prefetch abort*/
                                {
//                                        rpclog("Pref abort at %07X %i\n",PC,ins);
//                                        dumpregs();
//                                        exit(-1);
                                        templ=armregs[15];
                                        armregs[15]|=3;
                                        updatemode(SUPERVISOR);
                                        armregs[14]=templ;
                                        armregs[15]&=0xFC000003;
                                        armregs[15]|=0x08000010;
                                        refillpipeline();
                                        prefabort=0;
                                }
                                else if (databort==1)     /*Data abort*/
                                {
//                                        exit(-1);
/*                                        rpclog("Dat abort at %07X %i - opcode %08X R1 %08X R12 %08X\n",PC,ins,opcode,armregs[1],armregs[12]);
                                        if (output)
                                        {
                                                dumpregs();
                                                exit(-1);
                                        }*/
//                                        if (PC==0x21B2950) output=1;
//                                        dumpregs();
//                                        exit(-1);
//                                        output=0;
/*                                        if (!olog) olog=fopen("armlog.txt","wt");
                                        sprintf(err2,"Dat abort at %07X %i\n",PC,ins);
                                        fputs(err2,olog);
                                        dumpregs();
                                        exit(-1);*/
                                        templ=armregs[15];
                                        armregs[15]|=3;
                                        updatemode(SUPERVISOR);
                                        armregs[14]=templ;
                                        armregs[15]&=0xFC000003;
                                        armregs[15]|=0x08000014;
                                        refillpipeline();
                                        databort=0;
                                }
                                else if (databort==2) /*Address Exception*/
                                {
//                                        exit(-1);
//                                        rpclog("Address Exception at %07X %08X %07X\n",PC,opcode,opc);
//                                        dumpregs();
//                                        exit(-1);
/*                                        if (!olog) olog=fopen("armlog.txt","wt");
                                        sprintf(err2,"Address Exception at %07X %i %08X\n",PC,ins,opcode);
                                        fputs(err2,olog);*/
                                        templ=armregs[15];
                                        armregs[15]|=3;
                                        updatemode(SUPERVISOR);
                                        armregs[14]=templ;
                                        armregs[15]&=0xFC000003;
                                        armregs[15]|=0x08000018;
                                        refillpipeline();
                                        databort=0;
                                }
                                else if ((armirq&2) && !(armregs[15]&0x4000000)) /*FIQ*/
                                {
//                                        rpclog("FIQ %07X\n",PC);
//                                        if (output) rpclog("FIQ\n");
                                        templ=armregs[15];
                                        armregs[15]|=3;
                                        updatemode(FIQ);
                                        armregs[14]=templ;
                                        armregs[15]&=0xFC000001;
                                        armregs[15]|=0x0C000020;
                                        refillpipeline();
                                }
                                else if ((armirq&1) && !(armregs[15]&0x8000000)) /*IRQ*/
                                {
//                                        if (output) rpclog("IRQ\n");
/*                                        if (!olog) olog=fopen("armlog.txt","wt");
                                        sprintf(err2,"IRQ at %07X %i\n",PC,ins);
                                        fputs(err2,olog);*/
                                        templ=armregs[15];
                                        armregs[15]|=3;
                                        updatemode(IRQ);
                                        armregs[14]=templ;
                                        armregs[15]&=0xFC000002;
                                        armregs[15]|=0x0800001C;
                                        refillpipeline();
                                }
                        }
                        armirq=irq;
                        armregs[15]+=4;
                        if ((armregs[15]&3)!=mode) updatemode(armregs[15]&3);
//                        if (PC==0x18244F8) output=1;
//if (PC==0x181D478) output=1;
//                        if (PC==0x927C) output=1;
/*                        if (PC<12)
                        {
                                rpclog("Reset! %07X %08X\n",opc,PC);
                                dumpregs();
                                exit(-1);
                        }*/
/*                        if (PC==(0x1800E88+8)) rpclog("Hit protection! %07X\n",opc);
                        if (PC==(0x1800FEC+8)) rpclog("Hit reset! %07X\n",opc);*/
/*                        if (PC==0x8008)
                        {
                                rpclog("PC=8008!\n");
                                swioutput=1;
                        }*/
//                        inssinceswi++;
/*                        if (PC==(0x1823494+8)) rpclog("Hitted it\n");
                        if (PC==(0x8B24+8)) rpclog("Hit 8B24 %07X\n",opc);
                        if (PC>=0x29000 && PC<=0x29CA8) rpclog("Here! %07X %07X\n",PC,opc);
                        if (PC==(0x29CCC+8)) rpclog("PC 29CCC %08X\n",opc);
                        if (PC==(0x29CA0+8)) rpclog("PC 29CA0 %08X %08X %08X\n",armregs[0],armregs[1],armregs[12]);*/
//                        if (PC==(0x1815214+0x34c)) output=1;
//if (PC>=0x21B2934 && PC<=0x21B2950) output=1;
/*if (armregs[7]==0xdeadbeef)
{
        printf("We just got DEADBEEF\n");
                                rpclog("%05i : %07X %08X %08X %08X %08X %08X %08X %08X %08X",ins,PC,armregs[0],armregs[1],armregs[2],armregs[3],armregs[4],armregs[5],armregs[6],armregs[7]);
                                rpclog("  %08X %08X %08X %08X %08X %08X %08X %08X\n",armregs[8],armregs[9],armregs[10],armregs[11],armregs[12],armregs[13],armregs[14],armregs[15]);
                        printf("Opcode %08X\n",opcode);
                        exit(-1);
                }*/
//                if (PC==0x1815150) rpclog("1815148 from %08X %08X\n",opc,opcode);
//                if (PC==0x1814408) rpclog("here R0 %08X R12 %08X\n",armregs[0],armregs[12]);
//output=(PC>=0x185B5D4 && PC<0x185B8DC);
                        if (output && (PC-8) != 0x38E604c && (PC-8) != 0x38E6050 && (PC-8) != 0x38E6054)
                        {
                                rpclog("%05i : %07X %08X %08X %08X %08X %08X %08X %08X %08X",ins,PC,armregs[0],armregs[1],armregs[2],armregs[3],armregs[4],armregs[5],armregs[6],armregs[7]);
                                rpclog("  %08X %08X %08X %08X %08X %08X %08X %08X\n",armregs[8],armregs[9],armregs[10],armregs[11],armregs[12],armregs[13],armregs[14],armregs[15]);
//                                if (!olog) olog=fopen("olog.txt","wt");
//                                rpclog("%i : %07X %08X %08X %08X %08X %08X %08X\n\0 %08X %08X %08X %08X %08X\n\0 %08X %08X %08X %08X %08X %08X  %08X %08X %08X - %08X %08X %08X  %08X %08X\n",ins,PC,opc,armregs[0],armregs[1],armregs[2],armregs[4],armregs[5],opcode,armregs[12],armregs[0],armregs[1],armregs[2],armregs[3],armregs[4], opc, armregs[3],armregs[4],armregs[5],armregs[6],armregs[7],armregs[8],armregs[12],armregs[13],armregs[14],armregs[15],opcode,0,armregs[11],0);
                                ins++;
//                                fputs(err2,olog);
                        if (timetolive)
                        {
                                timetolive--;
                                if (!timetolive)
                                {
                                        dumpregs();
                                        exit(-1);
                                }
                        }
                        }
//                        if (output && ins>5000) exit(-1);
                        cyc=(oldcyc2-cycles);
                        ioc.timerc[0]-=(cyc<<1);
                        ioc.timerc[1]-=(cyc<<1);
                        if ((ioc.timerc[0]<0) || (ioc.timerc[1]<0)) updateioctimers();
                        if (motoron)
                        {
                                fdicount-=cyc;
                                if (fdicount<0)
                                {
                                        fdicount+=(speed>=2)?50*8:25*8;
                                        if (inreadop) fdinextbit();
                                        else          fdipos+=16;
                                }
                        }
                        linecyc-=(cyc<<1);
                        inscount++;
//                        ins++;
/*                        if (discint && fastdisc && speed<2)
                        {
                                discint-=cyc;
                                if (discint<1) { discint=0; callback(); }
                        }*/
                }
                cyc=vidcgetoverflow();
//                rpclog("Overflow %i  %i %i",cyc,ioc.timerc[0],ioc.timerc[1]);
                cycles-=(cyc>>1);
                ioc.timerc[0]-=cyc;
                ioc.timerc[1]-=cyc;
                if ((ioc.timerc[0]<0) || (ioc.timerc[1]<0)) updateioctimers();
//                rpclog("  %i %i\n",ioc.timerc[0],ioc.timerc[1]);
//                rpclog("Overflow %i samples per line %i\n",cyc,samplesperline);
                pollline();
                cyc=(oldcyc-cycles);
                soundtime-=cyc;//(cyc>>1);
                while (soundtime<0)
                {
                        samplesperline++;
                        pollsound();
                        if (!speed)        soundtime+=32;
                        else if (speed==1) soundtime+=48;
                        else if (speed==2) soundtime+=96;
                        else               soundtime+=128;
                }
/*                if (inreadop)
                {
                        fdicount-=(cyc>>1);
                        while (fdicount<0)
                        {
                                fdicount+=(speed==2)?24:8;
                                fdinextbit();
                        }
                }*/
                if (keydelay)
                {
                        keydelay-=256;
                        if (keydelay<=0) { keydelay=0; keycallback(); }
                }
                if (keydelay2)
                {
                        keydelay2-=256;
                        if (keydelay2<=0) { keydelay2=0; keycallback2(); }
                }
                if (discint)// && (!fastdisc || speed>1))
                {
                        discint-=128;
                        if (discint<1) { discint=0; callback(); }
                }
                if (fdccallback)// && !fastdisc)
                {
                        fdccallback-=300;
                        if (fdccallback<1) { fdccallback=0; callbackfdc(); }
                }
                if (idecallback)
                {
                        idecallback-=20;
                        if (idecallback<=0)
                        {
                                idecallback=0;
                                /*if (fdctype) */callbackide();
//                                else         callbackst506();
                        }
                }
                if (fdctype && motoron && discname[curdrive][0])
                {
                        fdci--;
                        if (fdci<=0)
                        {
                                fdci=5000;
                                ioc.irqa|=4;
                                updateirqs();
                        }
                        if (motoron>1) motoron-=2;
                }
                keyscount--;
                if (keyscount<0)
                {
                        updatekeys();
                        keyscount=200;
                }
                if (disccint && !fdctype)
                {
                        disccint-=128;
                        if (disccint<1) { disccint=0; discchangecint(); }
                }
        }
//        rpclog("End of frame %i\n",cycles);
}
