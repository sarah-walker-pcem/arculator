int podule_time=8000;

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
#include "disc.h"
#include <allegro.h>
#include "hostfs.h"
#include "keyboard.h"
#include "mem.h"
#include "memc.h"
#include "sound.h"
#include "vidc.h"
//#include <winalleg.h>

void refillpipeline();
void refillpipeline2();

int arm_dmacount = 0x7fffffff, arm_dmalatch = 0x7fffffff, arm_dmalength = 0;
static int cyc_s, cyc_n;

int arm_cpu_type;

int arm_cpu_speed, arm_mem_speed;
int arm_has_swp;
int arm_has_cp15;

int fpaena=0;
int swioutput=0;

int keyscount=100;
int idecallback;
int fdci=200;
int motoron;
int fdccallback;
int fdicount=16;
int irq;
int cycles;
int prefabort;
uint8_t flaglookup[16][16];
uint32_t rotatelookup[4096];
int disccint=0;
int timetolive=0;
int inscount;
int soundtime;
int discint;
int keydelay,keydelay2;
int armirq=0;
int output=0;

int ins=0;

int osmode=0;

static int mode;

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

static uint32_t pccache,*pccache2;
#define countbits(c) countbitstable[c]
static int countbitstable[65536];

void resetarm()
{
        int c,d,exec,data;
        uint32_t rotval,rotamount;
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
        FILE *f;
        uint32_t l;

        if (indumpregs) return;
        indumpregs=1;
        
        memmode=2;
        
/*        f=fopen("modules.dmp","wb");
        for (c=0x0000;c<0x100000;c+=4)
        {
                l=readmeml(c+0x1800000);
                putc(l,f);
                putc(l>>8,f);
                putc(l>>16,f);
                putc(l>>24,f);
        }
        fclose(f);
        f=fopen("ram.dmp","wb");
        for (c=0x0000;c<0x100000;c++)
            putc(readmemb(c),f);
        fclose(f);*/
        rpclog("R 0=%08X R 4=%08X R 8=%08X R12=%08X\nR 1=%08X R 5=%08X R 9=%08X R13=%08X\nR 2=%08X R 6=%08X R10=%08X R14=%08X\nR 3=%08X R 7=%08X R11=%08X R15=%08X\n%i %08X %08X\nf 8=%08X f 9=%08X f10=%08X f11=%08X\nf12=%08X f13=%08X f14=%08X",armregs[0],armregs[4],armregs[8],armregs[12],armregs[1],armregs[5],armregs[9],armregs[13],armregs[2],armregs[6],armregs[10],armregs[14],armregs[3],armregs[7],armregs[11],armregs[15],ins,opcode,opcode2,fiqregs[8],fiqregs[9],fiqregs[10],fiqregs[11],fiqregs[12],fiqregs[13],fiqregs[14]);
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

inline void setzn(uint32_t op)
{
        armregs[15]&=0x3FFFFFFF;
        if (!op)               armregs[15]|=ZFLAG;
        else if (checkneg(op)) armregs[15]|=NFLAG;
}

#define shift(o)  ((o&0xFF0)?shift3(o):armregs[RM])
#define shift2(o) ((o&0xFF0)?shift4(o):armregs[RM])

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
                if (memc_is_memc1) cycles -= (PC & 0xc) ? cyc_s : cyc_n;
                else if ((PC + 4) & 0xc) cycles--;
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
                if (memc_is_memc1) cycles -= (PC & 0xc) ? cyc_s : cyc_n;
                else if ((PC + 4) & 0xc) cycles--;
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
        }
}

inline unsigned rotate(unsigned data)
{
        uint32_t rotval;
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
        }
}

int ldrlookup[4]={0,8,16,24};

#define ldrresult(v,a) ((v>>ldrlookup[addr&3])|(v<<(32-ldrlookup[addr&3])))

#define undefined()\
                                                rpclog("Illegal instruction %08X\n",opcode); \
                                        templ=armregs[15]-4; \
                                        armregs[15]|=3;\
                                        updatemode(SUPERVISOR);\
                                        armregs[14]=templ;\
                                        armregs[15]&=0xFC000003;\
                                        armregs[15]|=0x08000008;\
                                        refillpipeline();\
                                        cycles--

#define readmemfff(addr,opcode) \
                        if ((addr>>12)==pccache) \
                           opcode=pccache2[(addr&0xFFF)>>2]; \
                        else \
                        { \
                                templ2=addr>>12; \
                                templ=memstat[addr>>12]; \
                                if (modepritabler[memmode][templ]) \
                                { \
                                        pccache=templ2; \
                                        pccache2=mempoint[templ2]; \
                                        opcode=mempoint[templ2][(addr&0xFFF)>>2]; \
                                        cyc_s = mem_speed[templ2 & 0x3fff][0];  \
                                        cyc_n = mem_speed[templ2 & 0x3fff][1];  \
                                } \
                                else \
                                { \
                                        opcode=readmemf(addr); \
                                        pccache=0xFFFFFFFF; \
                                } \
                        }

void refillpipeline()
{
        uint32_t templ,templ2,addr=PC-4;
//        if ((armregs[15]&0x3FFFFFC)==8) rpclog("illegal instruction %08X at %07X\n",opcode,opc);
        readmemfff(addr,opcode2);
        addr+=4;
        readmemfff(addr,opcode3);
}

void refillpipeline2()
{
        uint32_t templ,templ2,addr=PC-8;
        readmemfff(addr,opcode2);
        addr+=4;
        readmemfff(addr,opcode3);
}

int framecycs;

#define arm_mul_timing(v)       if (memc_is_memc1)                                              \
                                {                                                               \
                                        cycles--;                                               \
                                        if (v)       cycles -= cyc_n;                           \
                                        if (v >>  2) cycles -= cyc_n;                           \
                                        if (v >>  4) cycles -= cyc_n;                           \
                                        if (v >>  6) cycles -= cyc_n;                           \
                                        if (v >>  8) cycles -= cyc_n;                           \
                                        if (v >> 10) cycles -= cyc_n;                           \
                                        if (v >> 12) cycles -= cyc_n;                           \
                                        if (v >> 14) cycles -= cyc_n;                           \
                                        if (v >> 16) cycles -= cyc_n;                           \
                                        if (v >> 18) cycles -= cyc_n;                           \
                                        if (v >> 20) cycles -= cyc_n;                           \
                                        if (v >> 22) cycles -= cyc_n;                           \
                                        if (v >> 24) cycles -= cyc_n;                           \
                                        if (v >> 26) cycles -= cyc_n;                           \
                                        if (v >> 28) cycles -= cyc_n;                           \
                                        /*if (v >> 30) cycles -= cyc_n;*/                           \
                                        if (!(PC & 0xc))                                        \
                                                cycles -= (cyc_n - cyc_s);                      \
                                }                                                               \
                                else                                                            \
                                {                                                               \
                                        if ((PC + 4) & 0xc) cycles--;                           \
                                        ARM_POLLTIME();                                         \
                                        oldcyc2 = cycles;                                       \
                                        if (v)       cycles --;                                 \
                                        if (v >>  2) cycles --;                                 \
                                        if (v >>  4) cycles --;                                 \
                                        if (v >>  6) cycles --;                                 \
                                        if (v >>  8) cycles --;                                 \
                                        if (v >> 10) cycles --;                                 \
                                        if (v >> 12) cycles --;                                 \
                                        if (v >> 14) cycles --;                                 \
                                        if (v >> 16) cycles --;                                 \
                                        if (v >> 18) cycles --;                                 \
                                        if (v >> 20) cycles --;                                 \
                                        if (v >> 22) cycles --;                                 \
                                        if (v >> 24) cycles --;                                 \
                                        if (v >> 26) cycles --;                                 \
                                        if (v >> 28) cycles --;                                 \
                                        /*if (v >> 30) cycles --;*/                                 \
                                        ARM_POLLTIME_NODMA();                                   \
                                        arm_dmacount -= (cyc << 10);                            \
                                        if (arm_dmacount <= 0)                                  \
                                        {                                                       \
                                                arm_dmacount += arm_dmalatch;                   \
                                                arm_dmacount -= (arm_dmalength << 10);          \
                                        }                                                       \
                                        oldcyc2 = cycles;                                       \
                                }

#define ARM_POLLTIME()                                                          \
                        cyc = (oldcyc2 - cycles);                               \
                        arm_dmacount -= (cyc << 10);                            \
                        if (arm_dmacount <= 0)                                  \
                        {                                                       \
                                arm_dmacount += arm_dmalatch;                   \
                                cycles       -= arm_dmalength;                  \
                                arm_dmacount -= (arm_dmalength << 10);          \
                        }                                                       \
                        ARM_POLLTIME_NODMA();

#define ARM_POLLTIME_NODMA()                                                    \
                        cyc = (oldcyc2 - cycles);                               \
                        ioc.timerc[0] -= (cyc << 10);                           \
                        ioc.timerc[1] -= (cyc << 10);                          \
                        if ((ioc.timerc[0] < 0) || (ioc.timerc[1] < 0))         \
                                ioc_updatetimers();                             \
                                                                                \
                        if (fdc_time)                                           \
                        {                                                       \
                                fdc_time -= cyc;                                \
                                if (fdc_time <= 0)                              \
                                {                                               \
                                        fdc_time = 0;                           \
                                        fdc_callback();                         \
                                }                                               \
                        }                                                       \
                        if (motoron)                                            \
                        {                                                       \
                                disc_time -= cyc;                               \
                                if (disc_time <= 0)                             \
                                {                                               \
                                        disc_time += disc_poll_time;            \
                                        disc_poll();                            \
                                }                                               \
                        }                                                       \
                        linecyc -= (cyc << 10)

int refreshcount = 32;
void execarm(int cycs)
{
        uint32_t templ,templ2,mask,addr,addr2,*rn;
        int c,cyc,oldcyc,oldcyc2;
        int linecyc;

        cycles+=cycs;

        while (cycles>0)
        {
                oldcyc=cycles;
                linecyc=vidcgetcycs();
                framecycs+=linecyc;
                while (linecyc>0)
                {
                        opcode=opcode2;
                        opcode2=opcode3;
                        oldcyc2=cycles;
                        if ((PC>>12)==pccache)
                           opcode3=pccache2[(PC&0xFFF)>>2];
                        else
                        {
                                templ2=PC>>12;
                                templ=memstat[PC>>12];
                                if (modepritabler[memmode][templ])
                                {
                                        pccache=templ2;//PC>>15;
                                        pccache2=mempoint[templ2];
                                        opcode3=mempoint[templ2][(PC&0xFFF)>>2];
                                        cyc_s = mem_speed[templ2 & 0x3fff][0];
                                        cyc_n = mem_speed[templ2 & 0x3fff][1];
                                }
                                else
                                {
                                        opcode3=readmemf(PC);
                                        pccache=0xFFFFFFFF;
                                }
                        }
                        if (PC & 0xc)
                                cycles -= cyc_s;
                        else
                                cycles -= cyc_n;

                        /*if (!((PC)&0xC)) cycles--;*/
                        if (flaglookup[opcode>>28][armregs[15]>>28] && !prefabort)
                        {
                                switch ((opcode>>20)&0xFF)
                                {
                                        case 0x00: /*AND reg*/
                                        if (((opcode&0xF0)==0x90)) /*MUL*/
                                        {
                                                arm_mul_timing(armregs[MULRS]);
                                                armregs[MULRD]=(armregs[MULRM])*(armregs[MULRS]);
                                                if (MULRD==MULRM) armregs[MULRD]=0;
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
                                        }
                                        break;
                                        case 0x01: /*ANDS reg*/
                                        if (((opcode&0xF0)==0x90)) /*MULS*/
                                        {
                                                arm_mul_timing(armregs[MULRS]);
                                                armregs[MULRD]=(armregs[MULRM])*(armregs[MULRS]);
                                                if (MULRD==MULRM) armregs[MULRD]=0;
                                                setzn(armregs[MULRD]);
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
                                        }
                                        break;

                                        case 0x02: /*EOR reg*/
                                        if (((opcode&0xF0)==0x90)) /*MLA*/
                                        {
                                                arm_mul_timing(armregs[MULRS]);
                                                armregs[MULRD]=((armregs[MULRM])*(armregs[MULRS]))+armregs[MULRN];
                                                if (MULRD==MULRM) armregs[MULRD]=0;
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
                                        }
                                        break;
                                        case 0x03: /*EORS reg*/
                                        if (((opcode&0xF0)==0x90)) /*MLAS*/
                                        {
                                                arm_mul_timing(armregs[MULRS]);
                                                armregs[MULRD]=((armregs[MULRM])*(armregs[MULRS]))+armregs[MULRN];
                                                if (MULRD==MULRM) armregs[MULRD]=0;
                                                setzn(armregs[MULRD]);
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
                                        break;

                                        case 0x10: /*SWP word*/
                                        if (arm_has_swp && (opcode&0xF0)==0x90)
                                        {
                                                addr=GETADDR(RN);
                                                templ=GETREG(RM);
                                                LOADREG(RD,readmeml(addr));
                                                if (!databort) writememl(addr,templ);
                                                cycles -= cyc_n * 2;
                                        }
                                        else
                                        {
                                                templ=armregs[15]-4;
                                                armregs[15]|=3;
                                                updatemode(SUPERVISOR);
                                                armregs[14]=templ;
                                                armregs[15]&=0xFC000003;
                                                armregs[15]|=0x08000008;
                                                cycles-=3;
                                                refillpipeline();
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
                                        break;
                                        
                                        case 0x14: /*SWPB*/
                                        if (arm_has_swp)
                                        {
                                                addr=armregs[RN];
                                                templ=GETREG(RM);
                                                LOADREG(RD,readmemb(addr));
                                                writememb(addr,templ);
                                                cycles -= cyc_n * 2;
                                        }
                                        else
                                        {
                                                templ=armregs[15]-4;
                                                armregs[15]|=3;
                                                updatemode(SUPERVISOR);
                                                armregs[14]=templ;
                                                armregs[15]&=0xFC000003;
                                                armregs[15]|=0x08000008;
                                                cycles-=3;
                                                refillpipeline();
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
                                        break;

                                        case 0x1A: /*MOV reg*/
                                        if (RD==15)
                                        {
                                                armregs[15]=(armregs[15]&0xFC000003)|((shift2(opcode)+4)&0x3FFFFFC);
                                                refillpipeline();
                                        }
                                        else
                                           armregs[RD]=shift2(opcode);
/*                                        if (opcode == 0xe1a00000 && PC < 0x100000) 
                                                rpclog("%07X: MOV R0, R0 took %i cycles  %i  %i\n", PC, oldcyc2-cycles, vidc_displayon, refreshcount);*/
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
                                        break;

                                        case 0x1E: /*MVN reg*/
                                        if (RD==15)
                                        {
                                                armregs[15]=(armregs[15]&0xFC000003)|(((~shift2(opcode))+4)&0x3FFFFFC);
                                                refillpipeline();
                                        }
                                        else
                                           armregs[RD]=~shift2(opcode);
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
//                                                if (templ==0x3F0000 && RD==9 && RN==1) rpclog("Here! %07X %08X %08X\n",PC,armregs[6],armregs[5]);
                                                armregs[RD]=GETADDR(RN)&templ;
                                        }
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
                                        break;

                                        case 0x32:
/*                                                templ=armregs[15]-4;
                                                armregs[15]|=3;
                                                updatemode(SUPERVISOR);
                                                armregs[14]=templ;
                                                armregs[15]&=0xFC000003;
                                                armregs[15]|=0x08000008;*/
                                                cycles-=4;
//                                                refillpipeline();
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
                                        break;

                                        case 0x3A: /*MOV imm*/
                                        if (RD==15)
                                        {
                                                armregs[15]=(armregs[15]&0xFC000003)|((rotate2(opcode)+4)&0x3FFFFFC);
                                                refillpipeline();
                                        }
                                        else
                                           armregs[RD]=rotate2(opcode);
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
                                        break;

                                        case 0x3E: /*MVN imm*/
                                        if (RD==15)
                                        {
                                                armregs[15]=(armregs[15]&0xFC000003)|(((~rotate2(opcode))+4)&0x3FFFFFC);
                                                refillpipeline();
                                        }
                                        else
                                           armregs[RD]=~rotate2(opcode);
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
                                        break;

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
                                        cycles -= mem_speed[(addr >> 12) & 0x3fff][1];  /*1S + 1N*/
                                        if (memc_is_memc1) cycles -= cyc_n;             /* + 1N*/
                                        else if (PC & 0xc) cycles--;                    /* + 1I*/
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
                                        cycles -= mem_speed[(addr >> 12) & 0x3fff][1];  /*1S + 1N*/
                                        if (memc_is_memc1) cycles -= cyc_n;             /* + 1N*/
                                        else if (PC & 0xc) cycles--;                    /* + 1I*/
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
                                        cycles -= mem_speed[(addr >> 12) & 0x3fff][1];  /*1S + 1N*/
                                        if (memc_is_memc1) cycles -= cyc_n;             /* + 1N*/
                                        else if (PC & 0xc) cycles--;                    /* + 1I*/
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
                                        cycles -= mem_speed[(addr >> 12) & 0x3fff][1]; /*2N*/
                                        if (PC & 0xc) cycles -= cyc_n - cyc_s;
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
                                        cycles -= mem_speed[(addr >> 12) & 0x3fff][1]; /*2N*/
                                        if (PC & 0xc) cycles -= cyc_n - cyc_s;
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
                                        cycles -= mem_speed[(addr >> 12) & 0x3fff][1]; /*2N*/
                                        if (PC & 0xc) cycles -= cyc_n - cyc_s;
/*                                        if (opcode == 0xe58b0000 && PC < 0x100000) 
                                                rpclog("%07X: STR R0, [R11] took %i cycles\n", PC, oldcyc2-cycles);*/
                                        break;

                                        case 0x44: case 0x4C: case 0x64: case 0x6C: /*STRB Rd,[Rn],offset*/
                                        addr=GETADDR(RN);
                                        if (opcode&0x2000000) addr2=shiftmem(opcode);
                                        else                  addr2=opcode&0xFFF;
                                        writememb(addr,armregs[RD]);
                                        if (databort) break;
                                        if (opcode&0x800000) armregs[RN]+=addr2;
                                        else                 armregs[RN]-=addr2;
                                        cycles -= mem_speed[(addr >> 12) & 0x3fff][1]; /*2N*/
                                        if (PC & 0xc) cycles -= cyc_n - cyc_s;
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
                                        cycles -= mem_speed[(addr >> 12) & 0x3fff][1]; /*2N*/
                                        if (PC & 0xc) cycles -= cyc_n - cyc_s;
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
                                        cycles -= mem_speed[(addr >> 12) & 0x3fff][1]; /*2N*/
                                        if (PC & 0xc) cycles -= cyc_n - cyc_s;
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
                                        cycles -= mem_speed[(addr >> 12) & 0x3fff][1];  /*1S + 1N*/
                                        if (memc_is_memc1) cycles -= cyc_n;             /* + 1N*/
                                        else if (PC & 0xc) cycles--;                    /* + 1I*/
/*                                        if (opcode == 0xe59b0000 && PC < 0x100000) 
                                                rpclog("%07X: LDR R0, [R11] took %i cycles\n", PC, oldcyc2-cycles);*/
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
                                        cycles -= mem_speed[(addr >> 12) & 0x3fff][1];  /*1S + 1N*/
                                        if (memc_is_memc1) cycles -= cyc_n;             /* + 1N*/
                                        else if (PC & 0xc) cycles--;                    /* + 1I*/
                                        break;

#define STMfirst()      mask=1; \
                        for (c = 0; c < 15; c++) \
                        { \
                                if (opcode & mask) \
                                { \
                                        cycles -= mem_speed[(addr >> 12) & 0x3fff][1]; \
                                        if (c == 15) { writememl(addr, armregs[c] + 4); } \
                                        else         { writememl(addr, armregs[c]); } \
                                        addr += 4; \
                                        break; \
                                } \
                                mask <<= 1; \
                        } \
                        mask <<= 1; c++;
                        
#define STMall()        for (;c<15;c++) \
                        { \
                                if (opcode&mask) \
                                { \
                                        if (!(addr&0xC)) cycles -= mem_speed[(addr >> 12) & 0x3fff][1]; \
                                        else             cycles -= mem_speed[(addr >> 12) & 0x3fff][0]; \
                                        writememl(addr,armregs[c]); \
                                        addr+=4; \
                                } \
                                mask<<=1; \
                        } \
                        if (opcode&0x8000) \
                        { \
                                if (!(addr&0xC)) cycles -= mem_speed[(addr >> 12) & 0x3fff][1]; \
                                else             cycles -= mem_speed[(addr >> 12) & 0x3fff][0]; \
                                writememl(addr,armregs[15]+4); \
                        }

#define STMfirstS()     mask=1; \
                        for (c=0;c<15;c++) \
                        { \
                                if (opcode&mask) \
                                { \
                                        cycles -= mem_speed[(addr >> 12) & 0x3fff][1]; \
                                        if (c==15) { writememl(addr,armregs[c]+4); } \
                                        else       { writememl(addr,*usrregs[c]); } \
                                        addr+=4; \
                                        break; \
                                } \
                                mask<<=1; \
                        } \
                        mask<<=1; c++;

#define STMallS()       for (;c<15;c++) \
                        { \
                                if (opcode&mask) \
                                { \
                                        if (!(addr&0xC)) cycles -= mem_speed[(addr >> 12) & 0x3fff][1]; \
                                        else             cycles -= mem_speed[(addr >> 12) & 0x3fff][0]; \
                                        writememl(addr,*usrregs[c]); \
                                        addr+=4; \
                                } \
                                mask<<=1; \
                        } \
                        if (opcode&0x8000) \
                        { \
                                if (!(addr&0xC)) cycles -= mem_speed[(addr >> 12) & 0x3fff][1]; \
                                else             cycles -= mem_speed[(addr >> 12) & 0x3fff][0]; \
                                writememl(addr,armregs[15]+4); \
                        }

#define LDMall()        mask=1; \
                        if (addr & 0xc) cycles -= (mem_speed[(addr >> 12) & 0x3fff][1] - mem_speed[(addr >> 12) & 0x3fff][1]); \
                        for (c=0;c<15;c++) \
                        { \
                                if (opcode&mask) \
                                { \
                                        if (!(addr&0xC)) cycles -= mem_speed[(addr >> 12) & 0x3fff][1]; \
                                        else             cycles -= mem_speed[(addr >> 12) & 0x3fff][0]; \
                                        templ=readmeml(addr); if (!databort) armregs[c]=templ; \
                                        addr+=4; \
                                } \
                                mask<<=1; \
                        } \
                        if (opcode&0x8000) \
                        { \
                                if (!(addr&0xC)) cycles -= mem_speed[(addr >> 12) & 0x3fff][1]; \
                                else             cycles -= mem_speed[(addr >> 12) & 0x3fff][0]; \
                                templ=readmeml(addr); \
                                addr += 4; \
                                if (!databort) armregs[15]=(armregs[15]&0xFC000003)|((templ+4)&0x3FFFFFC); \
                                refillpipeline(); \
                        }

#define LDMallS()       mask=1; \
                        if (addr & 0xc) cycles -= (mem_speed[(addr >> 12) & 0x3fff][1] - mem_speed[(addr >> 12) & 0x3fff][1]); \
                        if (opcode&0x8000) \
                        { \
                                for (c=0;c<15;c++) \
                                { \
                                        if (opcode&mask) \
                                        { \
                                                if (!(addr & 0xC)) cycles -= mem_speed[(addr >> 12) & 0x3fff][1]; \
                                                else               cycles -= mem_speed[(addr >> 12) & 0x3fff][0]; \
                                                templ=readmeml(addr); if (!databort) armregs[c]=templ; \
                                                addr+=4; \
                                        } \
                                        mask<<=1; \
                                } \
                                if (!(addr & 0xC)) cycles -= mem_speed[(addr >> 12) & 0x3fff][1]; \
                                else               cycles -= mem_speed[(addr >> 12) & 0x3fff][0]; \
                                templ=readmeml(addr); \
                                addr += 4; \
                                if (!databort) \
                                { \
                                        if (armregs[15]&3) armregs[15]=(templ+4); \
                                        else               armregs[15]=(armregs[15]&0x0C000003)|((templ+4)&0xF3FFFFFC); \
                                } \
                                refillpipeline(); \
                        } \
                        else \
                        { \
                                for (c=0;c<15;c++) \
                                { \
                                        if (opcode&mask) \
                                        { \
                                                if (!(addr & 0xC)) cycles -= mem_speed[(addr >> 12) & 0x3fff][1]; \
                                                else               cycles -= mem_speed[(addr >> 12) & 0x3fff][0]; \
                                                templ=readmeml(addr); if (!databort) *usrregs[c]=templ; \
                                                addr+=4; \
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
                                        if (PC & 0xc) cycles -= cyc_n - cyc_s;
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
                                        if (PC & 0xc) cycles -= cyc_n - cyc_s;
/*                                        if (opcode == 0xe88b00ff && PC < 0x100000) 
                                                rpclog("%07X: STMIA R11, {R0 - R7} took %i cycles  %08X\n", PC, oldcyc2-cycles, armregs[11]);*/
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
                                        if (PC & 0xc) cycles -= cyc_n - cyc_s;
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
                                        if (PC & 0xc) cycles -= cyc_n - cyc_s;
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
					if (memc_is_memc1)
					{
						/*MEMC1 - repeat last cycle. */
						if (!((addr - 4) & 0xc))
							cycles -= cyc_n;
						else
							cycles -= cyc_s;
					}
					else
					{
                                        	if (PC & 0xc) cycles--;                         /* + 1I*/
					}
                                        break;
                                        case 0x89: /*LDMIA*/
                                        case 0x8B: /*LDMIA !*/
                                        case 0x99: /*LDMIB*/
                                        case 0x9B: /*LDMIB !*/
                                        addr=armregs[RN];
                                        if (opcode&0x1000000) addr+=4;
                                        if (opcode&0x200000) armregs[RN]+=countbits(opcode&0xFFFF);
                                        LDMall();
					if (memc_is_memc1)
					{
						/*MEMC1 - repeat last cycle. */
						if (!((addr - 4) & 0xc))
							cycles -= cyc_n;
						else
							cycles -= cyc_s;
					}
					else
					{
                                        	if (PC & 0xc) cycles--;                         /* + 1I*/
					}
                                        break;
                                        case 0x85: /*LDMDA ^*/
                                        case 0x87: /*LDMDA ^!*/
                                        case 0x95: /*LDMDB ^*/
                                        case 0x97: /*LDMDB ^!*/
                                        addr=armregs[RN]-countbits(opcode&0xFFFF);
                                        if (!(opcode&0x1000000)) addr+=4;
                                        if (opcode&0x200000) armregs[RN]-=countbits(opcode&0xFFFF);
                                        LDMallS();
					if (memc_is_memc1)
					{
						/*MEMC1 - repeat last cycle. */
						if (!((addr - 4) & 0xc))
							cycles -= cyc_n;
						else
							cycles -= cyc_s;
					}
					else
					{
                                        	if (PC & 0xc) cycles--;                         /* + 1I*/
					}
                                        break;
                                        case 0x8D: /*LDMIA ^*/
                                        case 0x8F: /*LDMIA ^!*/
                                        case 0x9D: /*LDMIB ^*/
                                        case 0x9F: /*LDMIB ^!*/
                                        addr=armregs[RN];
                                        if (opcode&0x1000000) addr+=4;
                                        if (opcode&0x200000) armregs[RN]+=countbits(opcode&0xFFFF);
                                        LDMallS();
					if (memc_is_memc1)
					{
						/*MEMC1 - repeat last cycle. */
						if (!((addr - 4) & 0xc))
							cycles -= cyc_n;
						else
							cycles -= cyc_s;
					}
					else
					{
                                        	if (PC & 0xc) cycles--;                         /* + 1I*/
					}
                                        break;

                                        case 0xB0: case 0xB1: case 0xB2: case 0xB3: /*BL*/
                                        case 0xB4: case 0xB5: case 0xB6: case 0xB7:
                                        case 0xB8: case 0xB9: case 0xBA: case 0xBB:
                                        case 0xBC: case 0xBD: case 0xBE: case 0xBF:
                                        templ=(opcode&0xFFFFFF)<<2;
                                        armregs[14]=armregs[15]-4;
                                        armregs[15]=((armregs[15]+templ+4)&0x3FFFFFC)|(armregs[15]&0xFC000003);
                                        refillpipeline();
                                        cycles -= (mem_speed[PC >> 12][1] + mem_speed[PC >> 12][0]);
                                        break;

                                        case 0xA0: case 0xA1: case 0xA2: case 0xA3: /*B*/
                                        case 0xA4: case 0xA5: case 0xA6: case 0xA7:
                                        case 0xA8: case 0xA9: case 0xAA: case 0xAB:
                                        case 0xAC: case 0xAD: case 0xAE: case 0xAF:
                                        templ=(opcode&0xFFFFFF)<<2;
                                        armregs[15]=((armregs[15]+templ+4)&0x3FFFFFC)|(armregs[15]&0xFC000003);
                                        refillpipeline();
                                        if (PC >= 4) cycles -= mem_speed[(PC - 4) >> 12][1];
                                        if (!(PC & 0xc)) 
                                                cycles -= mem_speed[PC >> 12][1];
                                        else
                                                cycles -= mem_speed[PC >> 12][0];
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
                                                        cycles -= (mem_speed[PC >> 12][1] + mem_speed[PC >> 12][0]);
                                                        refillpipeline();
                                                }
                                        }
                                        else if (MULRS==15 && (opcode&0x10) && arm_has_cp15)
                                        {
                                                writecp15(RN,armregs[RD]);
                                        }
                                        else
                                        {
//                                                rpclog("Illegal instruction %08X\n",opcode);
                                                templ=armregs[15]-4;
                                                armregs[15]|=3;
                                                updatemode(SUPERVISOR);
                                                armregs[14]=templ;
                                                armregs[15]&=0xFC000003;
                                                armregs[15]|=0x08000008;
                                                cycles -= (mem_speed[PC >> 12][1] + mem_speed[PC >> 12][0]);
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
                                                        cycles -= (mem_speed[PC >> 12][1] + mem_speed[PC >> 12][0]);
                                                        refillpipeline();
                                                }
                                        }
                                        else if (MULRS==15 && (opcode&0x10) && arm_has_cp15)
                                        {
                                                if (RD==15) armregs[RD]=(armregs[RD]&0x3FFFFFC)|(readcp15(RN)&0xFC000003);
                                                else        armregs[RD]=readcp15(RN);
                                        }
                                        else
                                        {
//                                                rpclog("Illegal instruction %08X\n",opcode);
                                                templ=armregs[15]-4;
                                                armregs[15]|=3;
                                                updatemode(SUPERVISOR);
                                                armregs[14]=templ;
                                                armregs[15]&=0xFC000003;
                                                armregs[15]|=0x08000008;
                                                cycles -= (mem_speed[PC >> 12][1] + mem_speed[PC >> 12][0]);
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
#if 0
                                        if ((opcode>=0xEC500000) && (opcode<=0xEC500008))
                                           arculfs(opcode&15);
                                        else 
#endif                                        
                                        if (((opcode&0xF00)==0x100 || (opcode&0xF00)==0x200) && fpaena)
                                        {
                                                if (fpaopcode(opcode))
                                                {
                                                        templ=armregs[15]-4;
                                                        armregs[15]|=3;
                                                        updatemode(SUPERVISOR);
                                                        armregs[14]=templ;
                                                        armregs[15]&=0xFC000003;
                                                        armregs[15]|=0x08000008;
                                                        cycles -= (mem_speed[PC >> 12][1] + mem_speed[PC >> 12][0]);
                                                        refillpipeline();
                                                }
                                        }
                                        else
                                        {
//                                                rpclog("Illegal instruction %08X\n",opcode);
                                                templ=armregs[15]-4;
                                                armregs[15]|=3;
                                                updatemode(SUPERVISOR);
                                                armregs[14]=templ;
                                                armregs[15]&=0xFC000003;
                                                armregs[15]|=0x08000008;
                                                cycles -= (mem_speed[PC >> 12][1] + mem_speed[PC >> 12][0]);
                                                refillpipeline();
                                        }
                                        break;

                                        case 0xF0: case 0xF1: case 0xF2: case 0xF3: /*SWI*/
                                        case 0xF4: case 0xF5: case 0xF6: case 0xF7:
                                        case 0xF8: case 0xF9: case 0xFA: case 0xFB:
                                        case 0xFC: case 0xFD: case 0xFE: case 0xFF:
                                        if (mousehack)
                                        {
                                                if ((opcode&0x1FFFF)==7 && armregs[0]==0x15 && (readmemb(armregs[1])==1))
                                                {
                                                        setmouseparams(armregs[1]);
                                                }
                                                else if ((opcode&0x1FFFF)==7 && armregs[0]==0x15 && (readmemb(armregs[1])==4))
                                                {
                                                        getunbufmouse(armregs[1]);
                                                }
                                                else if ((opcode&0x1FFFF)==7 && armregs[0]==0x15 && (readmemb(armregs[1])==3))
                                                {
                                                        setmousepos(armregs[1]);
                                                }
                                                else if ((opcode&0x1FFFF)==7 && armregs[0]==0x15 && (readmemb(armregs[1])==5))
                                                {
                                                        setmousepos(armregs[1]);
                                                }
                                        }
/*                                        if ((opcode&0x4FFFF) == 0x40240)
                                           rpclog("ADFS_DiscOp %08X %08X %08X %08X  %07X\n",armregs[1],armregs[2],armregs[3],armregs[4],PC);
                                        if ((opcode&0x4FFFF) == 0x40540)
                                           rpclog("FileCore_DiscOp %08X %08X %08X %08X  %07X\n",armregs[1],armregs[2],armregs[3],armregs[4],PC);*/
                                        if ((opcode & 0xdffff) == ARCEM_SWI_HOSTFS) 
                                        {
                                		ARMul_State state;

                                		state.Reg = armregs;
                                		templ = memmode;
                                		memmode = 2;
                                		hostfs(&state);
                                		memmode = templ;
                                	}
                                        else if ((opcode&0xFFFF)==0x1C && mousehack)
                                        {
                                                getosmouse();
                                                armregs[15]&=~VFLAG;
                                        }
                                        else
                                        {
/*                                                rpclog("SWI %05X at %07X  %08X %08X %08X %08X  %08X %08X %08X %08X %c\n",opcode&0x5FFFF,PC-8,armregs[0],armregs[1],armregs[2],armregs[3],armregs[4],armregs[5],armregs[6],armregs[7],(armregs[0]>31)?armregs[0]:'.');*/
                                                templ=armregs[15]-4;
                                                armregs[15]|=3;
                                                updatemode(SUPERVISOR);
                                                armregs[14]=templ;
                                                armregs[15]&=0xFC000003;
                                                armregs[15]|=0x0800000C;
                                                cycles -= (mem_speed[PC >> 12][1] + mem_speed[PC >> 12][0]);
                                                refillpipeline();
                                        }
                                        break;

                                        default:
                                                rpclog("Illegal instruction %08X %07X\n",opcode, PC);
                                        dumpregs();
                                        exit(-1);
                                                templ=armregs[15]-4;
                                                armregs[15]|=3;
                                                updatemode(SUPERVISOR);
                                                armregs[14]=templ;
                                                armregs[15]&=0xFC000003;
                                                armregs[15]|=0x08000008;
                                                cycles-=4;
                                                refillpipeline();
//                                        error("Bad opcode %02X %08X at %07X\n",(opcode>>20)&0xFF,opcode,PC);
//                                        dumpregs();
//                                        exit(-1);
                                }
                        }
                        else if (!prefabort)
                           cycles--;
                        if (databort|armirq|prefabort)
                        {
                                if (prefabort)       /*Prefetch abort*/
                                {
//                                        rpclog("Pref abort at %07X %i\n",PC,ins);
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
                                        rpclog("Dat abort at %07X %i - opcode %08X R1 %08X R11 %08X\n",PC,ins,opcode,armregs[1],armregs[11]);
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
//                                        rpclog("Address Exception at %07X %08X %08X %08X\n",PC,opcode,armregs[1],armregs[11]);
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
//                                        rpclog("FIQ %02X %i\n",ioc.fiq&ioc.mskf, cycles);
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
//                                        rpclog("IRQ %02X %02X\n",ioc.irqa&ioc.mska,ioc.irqb&ioc.mskb);
//                                        if (output) rpclog("IRQ\n");
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
/*                        if (PC == 0x8304)
                                rpclog("8304: R0 = %08X R8 = %08X\n", armregs[0], armregs[8]);
                        if (opcode == 0xE8BD000C)
                                rpclog("R0 = %08X R1 = %08X R2 = %08X R3 = %08X\n", armregs[0], armregs[1], armregs[2], armregs[3]);*/
/*output = 1;
                      if (PC == 0x381178C)
                              fatal("it\n");*/
//                        if (PC == 0x38E35CC)
//                           output = 1;

/*                        if (PC == 0x184A35C)
                                output = 1;
                        if (PC == 0x184A544)
                                output = 0;*/
                        if (output)
                        {
                                rpclog("%05i : %07X %08X %08X %08X %08X %08X %08X %08X %08X",ins,PC-8,armregs[0],armregs[1],armregs[2],armregs[3],armregs[4],armregs[5],armregs[6],armregs[7]);
                                rpclog("  %08X %08X %08X %08X %08X %08X %08X %08X  %08X  %02X %02X %02X  %02X %02X %02X  %i %i %i %X %i %i\n",armregs[8],armregs[9],armregs[10],armregs[11],armregs[12],armregs[13],armregs[14],armregs[15],opcode,ioc.mska,ioc.mskb,ioc.mskf,ioc.irqa,ioc.irqb,ioc.fiq,  fdc_indexcount, linecyc, motoron, fdc_indexpulse, motorspin, fdc_time);
                                ins++;

                                if (timetolive)
                                {
                                        timetolive--;
                                        if (!timetolive)
                                        {
                                                output=0;
//                                                dumpregs();
//                                                exit(-1);
                                        }
                                }
                        }

                        ARM_POLLTIME();
                        
                        inscount++;
                        ins++;
                }
       
                pollline();
                cyc=(oldcyc-cycles);
                podule_time-=cyc;
                while (podule_time<=0)
                {
                        podule_time += speed_mhz * 1000;
                        runpoduletimers(1);
                }

                soundtime-=cyc;//(cyc>>1);
                while (soundtime<0)
                {
                        soundtime += sound_poll_time;
                        pollsound();
                }
                keyboard_poll_count -= cyc;
                if (keyboard_poll_count < 0)
                {
                        keyboard_poll_count += keyboard_poll_time;
                        keyboard_poll();
                }
                if (key_rx_callback)
                {
                        key_rx_callback--;
                        if (!key_rx_callback)
                                key_do_rx_callback();
                }
                if (key_tx_callback)
                {
                        key_tx_callback--;
                        if (!key_tx_callback)
                                key_do_tx_callback();
                }
                if (idecallback)
                {
                        idecallback-=10;
                        if (idecallback<=0)
                        {
                                idecallback=0;
                                if (fdctype) callbackide();
                                else         callbackst506();
                        }
                }
/*                keyscount--;
                if (keyscount<0)
                {
                        updatekeys();
                        keyscount=200;
                }*/
        }
//        rpclog("End of frame %i\n",cycles);
}
