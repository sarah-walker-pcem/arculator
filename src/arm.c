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
#include <stdlib.h>
#include <string.h>
#include "arc.h"
#include "arcrom.h"
#include "arm.h"
#include "cp15.h"
#include "disc.h"
#include "fpa.h"
#include "hostfs.h"
#include "ide.h"
#include "ioc.h"
#include "keyboard.h"
#include "mem.h"
#include "memc.h"
#include "podules.h"
#include "sound.h"
#include "timer.h"
#include "vidc.h"

uint32_t armregs[16];

void refillpipeline();
void refillpipeline2();

static uint8_t arm3_cache[((1 << 26) >> 4) >> 3];
static uint32_t arm3_cache_tag[4][64];
static int arm3_slot = 1;
#define TAG_INVALID -1

static int cyc_s, cyc_n;

#define cyc_i (1 << 10)

uint32_t opcode;
static uint32_t opcode2,opcode3;
static uint32_t *usrregs[16],userregs[16],superregs[16],fiqregs[16],irqregs[16];

static int vidc_dma_pending = 0;
int vidc_fetches = 0;

int databort;
int prefabort, prefabort_next;

static void CLOCK_N(uint32_t addr)
{
//	rpclog("CLOCK_N %i %i %i\n", vidc_dma_pending, arm_dmacount, 0);
	if (vidc_dma_pending)
	{
		tsc += vidc_dma_pending;
		arm_dmacount -= vidc_dma_pending;
		vidc_dma_pending = 0;
	}
	arm_dmacount -= mem_speed[(addr >> 12) & 0x3fff][1];
	if (arm_dmacount <= 0)
	{
//		rpclog("arm_dmacount was %i ", arm_dmacount);
		arm_dmacount += vidc_update_cycles();
//		rpclog("now %i\n", arm_dmacount);
		vidc_dma_pending = vidc_dma_length;
		if (vidc_dma_pending)
			vidc_fetches++;
	}
}

static void CLOCK_S(uint32_t addr)
{
	arm_dmacount -= mem_speed[(addr >> 12) & 0x3fff][0];
	if (arm_dmacount <= 0)
	{
//		rpclog("arm_dmacount was %i ", arm_dmacount);
		arm_dmacount += vidc_update_cycles();
//		rpclog("now %i\n", arm_dmacount);
		vidc_dma_pending = vidc_dma_length;
		if (vidc_dma_pending)
			vidc_fetches++;
	}
}

static void CLOCK_I()
{
//	rpclog("CLOCK_I %i %i %i\n", vidc_dma_pending, arm_dmacount, 0);
	if (vidc_dma_pending)
	{
		vidc_dma_pending -= cyc_i;
		if (vidc_dma_pending < 0)
			vidc_dma_pending = 0;
	}
	arm_dmacount -= cyc_i;
	if (arm_dmacount <= 0)
	{
//		rpclog("arm_dmacount was %i ", arm_dmacount);
		arm_dmacount += vidc_update_cycles();
//		rpclog("now %i\n", arm_dmacount);
		vidc_dma_pending = vidc_dma_length;
		if (vidc_dma_pending)
			vidc_fetches++;
	}
}

void arm_clock_i(int i_cycles)
{
        while (i_cycles--)
        {
                CLOCK_I();
		tsc += cyc_i;
        }
}

void cache_read_timing(uint32_t addr, int is_n_cycle)
{
	int bit_offset = (addr >> 4) & 7;
	int byte_offset = ((addr & 0x3ffffff) >> (4+3));
	if (addr & 0xfc000000)
	       return; /*Address exception*/
	if (cp15_cacheon)
	{
		if (arm3_cache[byte_offset] & (1 << bit_offset))
		{
			CLOCK_I();
			tsc += cyc_i;
		}
		else if (!(arm3cp.cache & (1 << (addr >> 21))))
		{
			if (is_n_cycle)
				CLOCK_N(addr);
			else
				CLOCK_S(addr);
			tsc += is_n_cycle ? mem_speed[(addr >> 12) & 0x3fff][1] : mem_speed[(addr >> 12) & 0x3fff][0];
		}
		else
		{
			int set = (addr >> 4) & 3;
//			rpclog("cache fetch %08x\n", addr);
			if (arm3_cache_tag[set][arm3_slot] != TAG_INVALID)
			{
				int old_bit_offset = (arm3_cache_tag[set][arm3_slot] >> 4) & 7;
				int old_byte_offset = (arm3_cache_tag[set][arm3_slot] >> (4+3));
//				rpclog(" clear %i,%i\n", 
				arm3_cache[old_byte_offset] &= ~(1 << old_bit_offset);
			}
			
			arm3_cache_tag[set][arm3_slot] = addr & ~0xf;
			arm3_cache[byte_offset] |= (1 << bit_offset);
			CLOCK_N(addr);
			CLOCK_S(addr);
			CLOCK_S(addr);
			CLOCK_S(addr);
			tsc += mem_speed[(addr >> 12) & 0x3fff][1] + mem_speed[(addr >> 12) & 0x3fff][0]*3;
			if (!((arm3_slot ^ (arm3_slot >> 1)) & 1))
				arm3_slot |= 0x40;
			arm3_slot >>= 1;
		}
	}
	else
	{
//		rpclog("cycle %08x %i %i\n", addr, is_n_cycle, is_n_cycle ? mem_speed[(addr >> 12) & 0x3fff][1] : mem_speed[(addr >> 12) & 0x3fff][0]);
		if (is_n_cycle)
			CLOCK_N(addr);
		else
			CLOCK_S(addr);
		tsc += is_n_cycle ? mem_speed[(addr >> 12) & 0x3fff][1] : mem_speed[(addr >> 12) & 0x3fff][0];
	}
}

void cache_flush()
{
	int set, slot;
//	rpclog("cache_flush\n");
	for (set = 0; set < 4; set++)
	{
		for (slot = 0; slot < 64; slot++)
		{
			if (arm3_cache_tag[set][slot] != TAG_INVALID)
			{
				int bit_offset = (arm3_cache_tag[set][slot] >> 4) & 7;
				int byte_offset = (arm3_cache_tag[set][slot] >> (4+3));
				
				arm3_cache[byte_offset] &= ~(1 << bit_offset);
				
				arm3_cache_tag[set][arm3_slot] = TAG_INVALID;
			}
		}
	}
/*	for (set = 0; set < (((1 << 26) >> 4) >> 3); set++)
	{
		if (arm3_cache[set])
			fatal("Flush didn't flush - %x %x\n", set, arm3_cache[set]);
	}*/
}

void cache_write_timing(uint32_t addr, int is_n_cycle)
{
	if (addr & 0xfc000000)
	       return; /*Address exception*/
	if (arm3cp.disrupt & (1 << (addr >> 21)))
		cache_flush();
	if (is_n_cycle)
		CLOCK_N(addr);
	else
		CLOCK_S(addr);
	if (is_n_cycle)
 		tsc += mem_speed[(addr >> 12) & 0x3fff][1];
	else
		tsc += mem_speed[(addr >> 12) & 0x3fff][0];
}

int arm_dmacount = 0, arm_dmalatch = 0x7fffffff, arm_dmalength = 0;

int arm_cpu_type;

int arm_cpu_speed, arm_mem_speed;
int arm_has_swp;
int arm_has_cp15;

int fpaena=0;
int swioutput=0;

int keyscount=100;
int fdci=200;
int fdccallback;
int fdicount=16;
int irq;
uint8_t flaglookup[16][16];
static uint32_t rotatelookup[4096];
int disccint=0;
int timetolive=0;
int inscount;
int soundtime;
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
        int c,d,exec;
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

        /*Build rotatelookup table used by rotate2 macro, which rotates
          data[7:0] by data[11:8]<<1.*/
        uint32_t rotate_arg;
        for (rotate_arg=0;rotate_arg<4096;rotate_arg++)
        {
                /*Shifter overflow is undefined behaviour, and unpredictably
                  results in a broken lookup table on macOS, so we cast
                  everything to 64-bit types while doing the arithmetic.*/
                uint64_t rotval,rotamount;

                rotval=rotate_arg&0xFF;
                rotamount=((rotate_arg>>8)&0xF)<<1;
                rotval=(rotval>>rotamount)|(rotval<<(32-rotamount));
                rotatelookup[rotate_arg]=(uint32_t)rotval;
        }
        /*Sanity check for the above overflow case*/
        if (rotatelookup[1] != 1) {
                fatal("Sanity check failure: rotatelookup[1] == 0x%08X, should be 1\n", rotatelookup[1]);
        }

        armregs[15]=0x0C00000B;
        mode=3;
        memmode=2;
        memstat[0]=1;
        mempoint[0]=rom;
        refillpipeline2();
        resetcp15();
        resetfpa();

        memset(arm3_cache, 0, sizeof(arm3_cache));
        memset(arm3_cache_tag, TAG_INVALID, sizeof(arm3_cache_tag));
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
        
        f=fopen("modules.dmp","wb");
        for (c=0x0000;c<0x100000;c+=4)
        {
                l=readmeml(c+0x1800000);
                putc(l,f);
                putc(l>>8,f);
                putc(l>>16,f);
                putc(l>>24,f);
        }
        fclose(f);
/*        f=fopen("ram.dmp","wb");
        for (c=0x0000;c<0x100000;c++)
            putc(readmemb(c),f);
        fclose(f);*/
        rpclog("R 0=%08X R 4=%08X R 8=%08X R12=%08X\nR 1=%08X R 5=%08X R 9=%08X R13=%08X\nR 2=%08X R 6=%08X R10=%08X R14=%08X\nR 3=%08X R 7=%08X R11=%08X R15=%08X\n%i %08X %08X\nf 8=%08X f 9=%08X f10=%08X f11=%08X\nf12=%08X f13=%08X f14=%08X\n",armregs[0],armregs[4],armregs[8],armregs[12],armregs[1],armregs[5],armregs[9],armregs[13],armregs[2],armregs[6],armregs[10],armregs[14],armregs[3],armregs[7],armregs[11],armregs[15],ins,opcode,opcode2,fiqregs[8],fiqregs[9],fiqregs[10],fiqregs[11],fiqregs[12],fiqregs[13],fiqregs[14]);
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

static inline void setzn(uint32_t op)
{
        armregs[15]&=0x3FFFFFFF;
        if (!op)               armregs[15]|=ZFLAG;
        else if (checkneg(op)) armregs[15]|=NFLAG;
}

#define shift(o)         ((o & 0xff0) ? shift_long(o) : armregs[RM])
#define shift_noflags(o) ((o & 0xff0) ? shift_long_noflags(o) : armregs[RM])

static inline uint32_t shift_long(uint32_t opcode)
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
                if (memc_is_memc1)
		{
			if (PC & 0xc)
				CLOCK_S(PC);
			else
				CLOCK_N(PC);
			tsc += (PC & 0xc) ? cyc_s : cyc_n;
		}
                else if ((PC + 4) & 0xc)
		{
			CLOCK_I();
			tsc += cyc_i;
		}
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
        return 0;
}
static inline uint32_t shift_long_noflags(uint32_t opcode)
{
        int shiftmode=(opcode>>5)&3;
        int shiftamount=(opcode>>7)&31;
        uint32_t temp;
        int cflag=CFSET;
        
        if (!(opcode&0xFF0)) return armregs[RM];
        if (opcode&0x10)
        {
                shiftamount=armregs[(opcode>>8)&15]&0xFF;
                if (shiftmode==3)
                   shiftamount&=0x1F;
                if (memc_is_memc1)
		{
			if (PC & 0xc)
				CLOCK_S(PC);
			else
				CLOCK_N(PC);
			tsc += (PC & 0xc) ? cyc_s : cyc_n;
		}
                else if ((PC + 4) & 0xc)
		{
			CLOCK_I();
			tsc += cyc_i;
		}
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
        return 0;
}

static inline uint32_t rotate(uint32_t data)
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

#define rotate_noflags(v) rotatelookup[v & 4095]

static inline uint32_t shift_mem(uint32_t opcode)
{
        int shiftmode=(opcode>>5)&3;
        int shiftamount=(opcode>>7)&31;
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
        return 0;
}

int ldrlookup[4]={0,8,16,24};

#define ldrresult(v,a) ((v>>ldrlookup[addr&3])|(v<<(32-ldrlookup[addr&3])))

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
        
        prefabort_next = 0;
//        if ((armregs[15]&0x3FFFFFC)==8) rpclog("illegal instruction %08X at %07X\n",opcode,opc);
        readmemfff(addr,opcode2);
        addr+=4;
        prefabort = prefabort_next;
        readmemfff(addr,opcode3);

        cache_read_timing(PC-4, 1);
        cache_read_timing(PC, !(PC & 0xc));
}

void refillpipeline2()
{
        uint32_t templ,templ2,addr=PC-8;

        prefabort_next = 0;
        readmemfff(addr,opcode2);
        addr+=4;
        prefabort = prefabort_next;
        readmemfff(addr,opcode3);

        cache_read_timing(PC-8, 1);
        cache_read_timing(PC-4, !((PC-4) & 0xc));
}

int framecycs;

#define arm_mul_timing(v)       if (memc_is_memc1)                                              \
                                {                                                               \
                                        CLOCK_I(); tsc += cyc_i;                            \
                                        if (v)       { CLOCK_N(0); tsc += cyc_n; }            \
                                        if (v >>  2) { CLOCK_N(0); tsc += cyc_n; }            \
                                        if (v >>  4) { CLOCK_N(0); tsc += cyc_n; }            \
                                        if (v >>  6) { CLOCK_N(0); tsc += cyc_n; }            \
                                        if (v >>  8) { CLOCK_N(0); tsc += cyc_n; }            \
                                        if (v >> 10) { CLOCK_N(0); tsc += cyc_n; }            \
                                        if (v >> 12) { CLOCK_N(0); tsc += cyc_n; }            \
                                        if (v >> 14) { CLOCK_N(0); tsc += cyc_n; }            \
                                        if (v >> 16) { CLOCK_N(0); tsc += cyc_n; }            \
                                        if (v >> 18) { CLOCK_N(0); tsc += cyc_n; }            \
                                        if (v >> 20) { CLOCK_N(0); tsc += cyc_n; }            \
                                        if (v >> 22) { CLOCK_N(0); tsc += cyc_n; }            \
                                        if (v >> 24) { CLOCK_N(0); tsc += cyc_n; }            \
                                        if (v >> 26) { CLOCK_N(0); tsc += cyc_n; }            \
                                        if (v >> 28) { CLOCK_N(0); tsc += cyc_n; }            \
                                        if (!(PC & 0xc))                                        \
                                        { \
						CLOCK_I(); \
                                                tsc += cyc_i;                     \
					} \
                                }                                                               \
                                else                                                            \
                                {                                                               \
                                        if ((PC + 4) & 0xc) { CLOCK_I(); tsc += cyc_i; }                   \
                                        if (v)       { CLOCK_I(); tsc += cyc_i; }            \
                                        if (v >>  2) { CLOCK_I(); tsc += cyc_i; }            \
                                        if (v >>  4) { CLOCK_I(); tsc += cyc_i; }            \
                                        if (v >>  6) { CLOCK_I(); tsc += cyc_i; }            \
                                        if (v >>  8) { CLOCK_I(); tsc += cyc_i; }            \
                                        if (v >> 10) { CLOCK_I(); tsc += cyc_i; }            \
                                        if (v >> 12) { CLOCK_I(); tsc += cyc_i; }            \
                                        if (v >> 14) { CLOCK_I(); tsc += cyc_i; }            \
                                        if (v >> 16) { CLOCK_I(); tsc += cyc_i; }            \
                                        if (v >> 18) { CLOCK_I(); tsc += cyc_i; }            \
                                        if (v >> 20) { CLOCK_I(); tsc += cyc_i; }            \
                                        if (v >> 22) { CLOCK_I(); tsc += cyc_i; }            \
                                        if (v >> 24) { CLOCK_I(); tsc += cyc_i; }            \
                                        if (v >> 26) { CLOCK_I(); tsc += cyc_i; }            \
                                        if (v >> 28) { CLOCK_I(); tsc += cyc_i; }            \
                                }


static void exception(uint32_t vector, int new_mode, int pc_offset)
{
        uint32_t old_pc;

        old_pc = armregs[15] + pc_offset;
        armregs[15] &= 0xFC000000;
        armregs[15] |= 0x08000000 | vector | new_mode;
        if (new_mode == FIQ)
                armregs[15] |= 0x0c000000;
        updatemode(new_mode);
        armregs[14] = old_pc;
        refillpipeline();
}

#define EXCEPTION_UNDEFINED()  exception(0x08, SUPERVISOR, -4)
#define EXCEPTION_SWI()        exception(0x0c, SUPERVISOR, -4)
#define EXCEPTION_PREF_ABORT() exception(0x10, SUPERVISOR, 0)
#define EXCEPTION_DATA_ABORT() exception(0x14, SUPERVISOR, 0)
#define EXCEPTION_ADDRESS()    exception(0x18, SUPERVISOR, 0)
#define EXCEPTION_IRQ()        exception(0x1c, IRQ, 0)
#define EXCEPTION_FIQ()        exception(0x20, FIQ, 0)

#define LOAD_R15(v) do \
                { \
                        armregs[15] = (((v) + 4) & 0x3fffffc) | (armregs[15] & 0xfc000003); \
                        refillpipeline(); \
                } while (0)

#define LOAD_R15_S(v) do \
                { \
                        if (armregs[15] & 3) \
                        { \
                                armregs[15] = (v) + 4; \
                                if ((armregs[15] & 3) != mode) \
                                        updatemode(armregs[15] & 3); \
                        } \
                        else \
                                armregs[15] = (((v) + 4) & 0xf3fffffc) | (armregs[15] & 0x0c000003); \
                        refillpipeline(); \
                } while (0)

#define CHECK_ADDR_EXCEPTION(a) if ((a) & 0xfc000000) { databort = 2; break; }

int refreshcount = 32;
static int total_cycles;

/*Execute ARM instructions for `cycs` clock ticks, typically 10 ms
  (cycs=80k for an 8MHz ARM2).*/
void execarm(int cycles_to_execute)
{
        uint32_t templ,templ2,mask,addr,addr2;
        int c;
        int cyc; /*Number of clock ticks executed in the last loop*/
        int oldcyc;

        int clock_ticks_executed = 0;
        
        LOG_EVENT_LOOP("execarm(%d) total_cycles=%i\n", cycles_to_execute, total_cycles);

        total_cycles+=cycles_to_execute << 10;

        while (total_cycles>0)
        {
//                LOG_VIDC_TIMING("cycles (%d) += vidcgetcycs() (%d) --> %d (%d) to execute before pollline()\n",
//                        oldcyc, vidc_cycles_to_execute, 0, 0);
                oldcyc = (int)tsc;

                opcode = opcode2;
                opcode2 = opcode3;
                if ((PC >> 12) == pccache)
                        opcode3 = pccache2[(PC & 0xFFF) >> 2];
                else
                {
                        templ2 = PC >> 12;
                        templ = memstat[PC >> 12];
                        if (modepritabler[memmode][templ])
                        {
                                pccache = templ2;
                                pccache2 = mempoint[templ2];
                                opcode3 = mempoint[templ2][(PC & 0xFFF) >> 2];
                                cyc_s = mem_speed[templ2 & 0x3fff][0];
                                cyc_n = mem_speed[templ2 & 0x3fff][1];
                        }
                        else
                        {
                                opcode3 = readmemf(PC);
                                pccache = 0xFFFFFFFF;
                        }
                }
                cache_read_timing(PC, (PC & 0xc) ? 0 : 1);

                if (flaglookup[opcode >> 28][armregs[15] >> 28] && !prefabort)
                {
                        switch ((opcode >> 20) & 0xFF)
                        {
                                case 0x00: /*AND reg*/
                                if ((opcode&0xF0) == 0x90) /*MUL*/
                                {
                                        arm_mul_timing(armregs[MULRS]);
                                        armregs[MULRD] = (armregs[MULRM]) * (armregs[MULRS]);
                                        if (MULRD == MULRM)
                                                armregs[MULRD] = 0;
                                }
                                else
                                {
                                        if (RD == 15)
                                        {
                                                templ = shift_noflags(opcode);
                                                LOAD_R15(GETADDR(RN) & templ);
                                        }
                                        else
                                        {
                                                templ = shift_noflags(opcode);
                                                armregs[RD] = GETADDR(RN) & templ;
                                        }
                                }
                                break;
                                case 0x01: /*ANDS reg*/
                                if ((opcode & 0xF0) == 0x90) /*MULS*/
                                {
                                        arm_mul_timing(armregs[MULRS]);
                                        armregs[MULRD] = (armregs[MULRM]) * (armregs[MULRS]);
                                        if (MULRD == MULRM)
                                                armregs[MULRD]=0;
                                        setzn(armregs[MULRD]);
                                }
                                else
                                {
                                        if (RD == 15)
                                        {
                                                templ = shift_noflags(opcode);
                                                LOAD_R15_S(GETADDR(RN) & templ);
                                        }
                                        else
                                        {
                                                templ = shift(opcode);
                                                armregs[RD] = GETADDR(RN) & templ;
                                                setzn(armregs[RD]);
                                        }
                                }
                                break;

                                case 0x02: /*EOR reg*/
                                if ((opcode & 0xF0) == 0x90) /*MLA*/
                                {
                                        arm_mul_timing(armregs[MULRS]);
                                        armregs[MULRD] = ((armregs[MULRM]) * (armregs[MULRS])) + armregs[MULRN];
                                        if (MULRD == MULRM)
                                                armregs[MULRD] = 0;
                                }
                                else
                                {
                                        if (RD == 15)
                                        {
                                                templ = shift_noflags(opcode);
                                                LOAD_R15(GETADDR(RN) ^ templ);
                                        }
                                        else
                                        {
                                                templ = shift_noflags(opcode);
                                                armregs[RD] = GETADDR(RN) ^ templ;
                                        }
                                }
                                break;
                                case 0x03: /*EORS reg*/
                                if ((opcode & 0xF0) == 0x90) /*MLAS*/
                                {
                                        arm_mul_timing(armregs[MULRS]);
                                        armregs[MULRD] = ((armregs[MULRM]) * (armregs[MULRS])) + armregs[MULRN];
                                        if (MULRD == MULRM)
                                                armregs[MULRD] = 0;
                                        setzn(armregs[MULRD]);
                                }
                                else
                                {
                                        if (RD == 15)
                                        {
                                                templ = shift_noflags(opcode);
                                                LOAD_R15_S(GETADDR(RN) ^ templ);
                                        }
                                        else
                                        {
                                                templ = shift(opcode);
                                                armregs[RD] = GETADDR(RN) ^ templ;
                                                setzn(armregs[RD]);
                                        }
                                }
                                break;

                                case 0x04: /*SUB reg*/
                                if (RD == 15)
                                {
                                        templ = shift_noflags(opcode);
                                        LOAD_R15(GETADDR(RN) - templ);
                                }
                                else
                                {
                                        templ = shift_noflags(opcode);
                                        armregs[RD] = GETADDR(RN) - templ;
                                }
                                break;
                                case 0x05: /*SUBS reg*/
                                if (RD == 15)
                                {
                                        templ = shift_noflags(opcode);
                                        LOAD_R15_S(GETADDR(RN) - templ);
                                }
                                else
                                {
                                        templ = shift_noflags(opcode);
                                        setsub(GETADDR(RN), templ, GETADDR(RN) - templ);
                                        armregs[RD] = GETADDR(RN) - templ;
                                }
                                break;

                                case 0x06: /*RSB reg*/
                                if (RD == 15)
                                {
                                        templ = shift_noflags(opcode);
                                        LOAD_R15(templ - GETADDR(RN));
                                }
                                else
                                {
                                        templ = shift_noflags(opcode);
                                        armregs[RD] = templ - GETADDR(RN);
                                }
                                break;
                                case 0x07: /*RSBS reg*/
                                if (RD == 15)
                                {
                                        templ = shift_noflags(opcode);
                                        LOAD_R15_S(templ - GETADDR(RN));
                                }
                                else
                                {
                                        templ = shift_noflags(opcode);
                                        setsub(templ, GETADDR(RN), templ - GETADDR(RN));
                                        armregs[RD] = templ - GETADDR(RN);
                                }
                                break;

                                case 0x08: /*ADD reg*/
                                if (RD == 15)
                                {
                                        templ = shift_noflags(opcode);
                                        LOAD_R15(GETADDR(RN) + templ);
                                }
                                else
                                {
                                        templ = shift_noflags(opcode);
                                        armregs[RD] = GETADDR(RN) + templ;
                                }
                                break;
                                case 0x09: /*ADDS reg*/
                                if (RD == 15)
                                {
                                        templ = shift_noflags(opcode);
                                        LOAD_R15_S(GETADDR(RN) + templ);
                                        refillpipeline();
                                }
                                else
                                {
                                        templ = shift_noflags(opcode);
                                        setadd(GETADDR(RN), templ, GETADDR(RN) + templ);
                                        armregs[RD] = GETADDR(RN) + templ;
                                }
                                break;
                        
                                case 0x0A: /*ADC reg*/
                                if (RD == 15)
                                {
                                        templ2 = CFSET;
                                        templ = shift_noflags(opcode);
                                        LOAD_R15(GETADDR(RN) + templ + templ2);
                                }
                                else
                                {
                                        templ2 = CFSET;
                                        templ = shift_noflags(opcode);
                                        armregs[RD] = GETADDR(RN) + templ + templ2;
                                }
                                break;
                                case 0x0B: /*ADCS reg*/
                                if (RD == 15)
                                {
                                        templ2 = CFSET;
                                        templ = shift_noflags(opcode);
                                        LOAD_R15_S(GETADDR(RN) + templ + templ2);
                                }
                                else
                                {
                                        templ2 = CFSET;
                                        templ = shift_noflags(opcode);
                                        setadc(GETADDR(RN), templ, GETADDR(RN) + templ + templ2);
                                        armregs[RD] = GETADDR(RN) + templ + templ2;
                                }
                                break;

                                case 0x0C: /*SBC reg*/
                                templ2 = (CFSET) ? 0 : 1;
                                if (RD == 15)
                                {
                                        templ = shift_noflags(opcode);
                                        LOAD_R15(GETADDR(RN) - (templ + templ2));
                                }
                                else
                                {
                                        templ = shift_noflags(opcode);
                                        armregs[RD] = GETADDR(RN) - (templ + templ2);
                                }
                                break;
                                case 0x0D: /*SBCS reg*/
                                templ2 = (CFSET) ? 0 : 1;
                                if (RD == 15)
                                {
                                        templ = shift_noflags(opcode);
                                        LOAD_R15_S(GETADDR(RN) - (templ + templ2));
                                }
                                else
                                {
                                        templ = shift_noflags(opcode);
                                        setsbc(GETADDR(RN), templ, GETADDR(RN) - (templ + templ2));
                                        armregs[RD] = GETADDR(RN) - (templ + templ2);
                                }
                                break;
                                case 0x0E: /*RSC reg*/
                                templ2 = (CFSET) ? 0 : 1;
                                if (RD == 15)
                                {
                                        templ = shift_noflags(opcode);
                                        LOAD_R15(templ - (GETADDR(RN) + templ2));
                                }
                                else
                                {
                                        templ = shift_noflags(opcode);
                                        armregs[RD] = templ - (GETADDR(RN) + templ2);
                                }
                                break;
                                case 0x0F: /*RSCS reg*/
                                templ2 = (CFSET) ? 0 : 1;
                                if (RD == 15)
                                {
                                        templ = shift_noflags(opcode);
                                        LOAD_R15_S(templ - (GETADDR(RN) + templ2));
                                }
                                else
                                {
                                        templ = shift_noflags(opcode);
                                        setsbc(templ, GETADDR(RN), templ - (GETADDR(RN) + templ2));
                                        armregs[RD] = templ - (GETADDR(RN) + templ2);
                                }
                                break;

                                case 0x10: /*SWP word*/
                                if ((opcode & 0xf0) != 0x90)
                                	break;
                                if (arm_has_swp)
                                {
                                        addr = GETADDR(RN);
                                        CHECK_ADDR_EXCEPTION(addr);
                                        templ = GETREG(RM);
                                        LOADREG(RD, readmeml(addr));
                                        cache_read_timing(PC, 1);
                                        if (!databort)
                                        {
                                                writememl(addr, templ);
                                        }
                                        cache_write_timing(addr, 1);
                                        CLOCK_N(addr);
                                        CLOCK_N(addr);
                                        tsc += cyc_n * 2;
                                }
                                else
                                        EXCEPTION_UNDEFINED();
                                break;
                                
                                case 0x11: /*TST reg*/
                                if (RD == 15)
                                {
                                        if (armregs[15] & 3)
                                        {
                                                templ = armregs[15] & 0x3FFFFFC;
                                                armregs[15] = ((GETADDR(RN) & shift_noflags(opcode)) & 0xFC000003)|templ;
                                        }
                                        else
                                        {
                                                templ = armregs[15] & 0x0FFFFFFF;
                                                armregs[15] = ((GETADDR(RN) & shift_noflags(opcode))&0xF0000000)|templ;
                                        }
                                }
                                else
                                {
                                        setzn(GETADDR(RN) & shift(opcode));
                                }
                                break;

                                case 0x12:
                                break;
                                
                                case 0x13: /*TEQ reg*/
                                if (RD == 15)
                                {
                                        if (armregs[15] & 3)
                                        {
                                                templ = armregs[15] & 0x3FFFFFC;
                                                armregs[15] = ((GETADDR(RN) ^ shift_noflags(opcode)) & 0xFC000003) | templ;
                                        }
                                        else
                                        {
                                                templ = armregs[15] & 0x0FFFFFFF;
                                                armregs[15] = ((GETADDR(RN) ^ shift_noflags(opcode)) & 0xF0000000) | templ;
                                        }
                                }
                                else
                                {
                                        setzn(GETADDR(RN) ^ shift(opcode));
                                }
                                break;
                                
                                case 0x14: /*SWPB*/
                                if ((opcode & 0xf0) != 0x90)
                                	break;
                                if (arm_has_swp)
                                {
                                        addr = armregs[RN];
                                        CHECK_ADDR_EXCEPTION(addr);
                                        templ = GETREG(RM);
                                        LOADREG(RD, readmemb(addr));
                                        cache_read_timing(addr, 1);
                                        writememb(addr, templ);
                                        cache_write_timing(addr, 1);
                                        CLOCK_N(addr);
                                        CLOCK_N(addr);
                                        tsc += cyc_n * 2;
                                }
                                else
                                        EXCEPTION_UNDEFINED();
                                break;

                                case 0x15: /*CMP reg*/
                                if (RD == 15)
                                {
                                        if (armregs[15] & 3)
                                        {
                                                templ = armregs[15] & 0x3FFFFFC;
                                                armregs[15] = ((GETADDR(RN) - shift_noflags(opcode)) & 0xFC000003) | templ;
                                        }
                                        else
                                        {
                                                templ = armregs[15] & 0x0FFFFFFF;
                                                armregs[15] = ((GETADDR(RN) - shift_noflags(opcode)) & 0xF0000000) | templ;
                                        }
                                }
                                else
                                        setsub(GETADDR(RN), shift(opcode), GETADDR(RN) - shift_noflags(opcode));
                                break;

                                case 0x17: /*CMN reg*/
                                if (RD == 15)
                                {
                                        if (armregs[15] & 3)
                                        {
                                                templ = armregs[15] & 0x3FFFFFC;
                                                armregs[15] = ((GETADDR(RN) + shift_noflags(opcode)) & 0xFC000003) | templ;
                                        }
                                        else
                                        {
                                                templ = armregs[15] & 0x0FFFFFFF;
                                                armregs[15] = ((GETADDR(RN) + shift_noflags(opcode)) & 0xF0000000) | templ;
                                        }
                                }
                                else
                                        setadd(GETADDR(RN), shift_noflags(opcode), GETADDR(RN) + shift_noflags(opcode));
                                break;

                                case 0x18: /*ORR reg*/
                                if (RD == 15)
                                {
                                        templ = shift_noflags(opcode);
                                        LOAD_R15(GETADDR(RN) | templ);
                                }
                                else
                                {
                                        templ = shift_noflags(opcode);
                                        armregs[RD] = GETADDR(RN) | templ;
                                }
                                break;
                                case 0x19: /*ORRS reg*/
                                if (RD == 15)
                                {
                                        templ = shift_noflags(opcode);
                                        LOAD_R15_S(GETADDR(RN) | templ);
                                }
                                else
                                {
                                        templ = shift(opcode);
                                        armregs[RD] = GETADDR(RN) | templ;
                                        setzn(armregs[RD]);
                                }
                                break;

                                case 0x1A: /*MOV reg*/
                                if (RD == 15)
                                        LOAD_R15(shift_noflags(opcode));
                                else
                                        armregs[RD] = shift_noflags(opcode);
                                break;
                                case 0x1B: /*MOVS reg*/
                                if (RD == 15)
                                        LOAD_R15_S(shift_noflags(opcode));
                                else
                                {
                                        armregs[RD] = shift(opcode);
                                        setzn(armregs[RD]);
                                }
                                break;

                                case 0x1C: /*BIC reg*/
                                if (RD == 15)
                                {
                                        templ = shift_noflags(opcode);
                                        LOAD_R15(GETADDR(RN) & ~templ);
                                }
                                else
                                {
                                        templ = shift_noflags(opcode);
                                        armregs[RD] = GETADDR(RN) & ~templ;
                                }
                                break;
                                case 0x1D: /*BICS reg*/
                                if (RD == 15)
                                {
                                        templ = shift_noflags(opcode);
                                        LOAD_R15_S(GETADDR(RN) & ~templ);
                                }
                                else
                                {
                                        templ = shift(opcode);
                                        armregs[RD] = GETADDR(RN) & ~templ;
                                        setzn(armregs[RD]);
                                }
                                break;

                                case 0x1E: /*MVN reg*/
                                if (RD == 15)
                                        LOAD_R15(~shift_noflags(opcode));
                                else
                                        armregs[RD] = ~shift_noflags(opcode);
                                break;
                                case 0x1F: /*MVNS reg*/
                                if (RD == 15)
                                        LOAD_R15_S(~shift_noflags(opcode));
                                else
                                {
                                        armregs[RD] = ~shift(opcode);
                                        setzn(armregs[RD]);
                                }
                                break;

                                case 0x20: /*AND imm*/
                                if (RD == 15)
                                {
                                        templ = rotate_noflags(opcode);
                                        LOAD_R15(GETADDR(RN) & templ);
                                }
                                else
                                {
                                        templ = rotate_noflags(opcode);
                                        armregs[RD] = GETADDR(RN) & templ;
                                }
                                break;
                                case 0x21: /*ANDS imm*/
                                if (RD == 15)
                                {
                                        templ = rotate_noflags(opcode);
                                        LOAD_R15_S(GETADDR(RN) & templ);
                                }
                                else
                                {
                                        templ = rotate(opcode);
                                        armregs[RD] = GETADDR(RN) & templ;
                                        setzn(armregs[RD]);
                                }
                                break;

                                case 0x22: /*EOR imm*/
                                if (RD == 15)
                                {
                                        templ = rotate_noflags(opcode);
                                        LOAD_R15(GETADDR(RN) ^ templ);
                                }
                                else
                                {
                                        templ = rotate_noflags(opcode);
                                        armregs[RD] = GETADDR(RN) ^ templ;
                                }
                                break;
                                case 0x23: /*EORS imm*/
                                if (RD == 15)
                                {
                                        templ = rotate_noflags(opcode);
                                        LOAD_R15_S(GETADDR(RN) ^ templ);
                                }
                                else
                                {
                                        templ = rotate(opcode);
                                        armregs[RD] = GETADDR(RN) ^ templ;
                                        setzn(armregs[RD]);
                                }
                                break;

                                case 0x24: /*SUB imm*/
                                if (RD == 15)
                                {
                                        templ = rotate_noflags(opcode);
                                        LOAD_R15(GETADDR(RN) - templ);
                                }
                                else
                                {
                                        templ = rotate_noflags(opcode);
                                        armregs[RD] = GETADDR(RN) - templ;
                                }
                                break;
                                case 0x25: /*SUBS imm*/
                                if (RD == 15)
                                {
                                        templ = rotate_noflags(opcode);
                                        LOAD_R15_S(GETADDR(RN) - templ);
                                }
                                else
                                {
                                        templ = rotate_noflags(opcode);
                                        setsub(GETADDR(RN), templ, GETADDR(RN) - templ);
                                        armregs[RD] = GETADDR(RN) - templ;
                                }
                                break;

                                case 0x26: /*RSB imm*/
                                if (RD == 15)
                                {
                                        templ = rotate_noflags(opcode);
                                        LOAD_R15(templ - GETADDR(RN));
                                }
                                else
                                {
                                        templ = rotate_noflags(opcode);
                                        armregs[RD] = templ - GETADDR(RN);
                                }
                                break;
                                case 0x27: /*RSBS imm*/
                                if (RD == 15)
                                {
                                        templ = rotate_noflags(opcode);
                                        LOAD_R15_S(templ - GETADDR(RN));
                                }
                                else
                                {
                                        templ = rotate_noflags(opcode);
                                        setsub(templ, GETADDR(RN), templ - GETADDR(RN));
                                        armregs[RD] = templ - GETADDR(RN);
                                }
                                break;

                                case 0x28: /*ADD imm*/
                                if (RD == 15)
                                {
                                        templ = rotate_noflags(opcode);
                                        LOAD_R15(GETADDR(RN) + templ);
                                }
                                else
                                {
                                        templ = rotate_noflags(opcode);
                                        armregs[RD] = GETADDR(RN) + templ;
                                }
                                break;
                                case 0x29: /*ADDS imm*/
                                if (RD == 15)
                                {
                                        templ = rotate_noflags(opcode);
                                        LOAD_R15_S(GETADDR(RN) + templ);
                                }
                                else
                                {
                                        templ = rotate_noflags(opcode);
                                        setadd(GETADDR(RN), templ, GETADDR(RN) + templ);
                                        armregs[RD] = GETADDR(RN) + templ;
                                }
                                break;

                                case 0x2A: /*ADC imm*/
                                if (RD == 15)
                                {
                                        templ2 = CFSET;
                                        templ = rotate_noflags(opcode);
                                        LOAD_R15(GETADDR(RN) + templ + templ2);
                                }
                                else
                                {
                                        templ2 = CFSET;
                                        templ = rotate_noflags(opcode);
                                        armregs[RD] = GETADDR(RN) + templ + templ2;
                                }
                                break;
                                case 0x2B: /*ADCS imm*/
                                if (RD == 15)
                                {
                                        templ2 = CFSET;
                                        templ = rotate_noflags(opcode);
                                        LOAD_R15_S(GETADDR(RN) + templ + templ2);
                                }
                                else
                                {
                                        templ2 = CFSET;
                                        templ = rotate_noflags(opcode);
                                        setadc(GETADDR(RN), templ,GETADDR(RN) + templ + templ2);
                                        armregs[RD] = GETADDR(RN) + templ + templ2;
                                }
                                break;

                                case 0x2C: /*SBC imm*/
                                templ2 = (CFSET) ? 0 : 1;
                                if (RD == 15)
                                {
                                        templ = rotate_noflags(opcode);
                                        LOAD_R15(GETADDR(RN) - (templ + templ2));
                                }
                                else
                                {
                                        templ = rotate_noflags(opcode);
                                        armregs[RD] = GETADDR(RN) - (templ + templ2);
                                }
                                break;
                                case 0x2D: /*SBCS imm*/
                                templ2 = (CFSET) ? 0 : 1;
                                if (RD == 15)
                                {
                                        templ = rotate_noflags(opcode);
                                        LOAD_R15_S(GETADDR(RN) - (templ + templ2));
                                }
                                else
                                {
                                        templ = rotate_noflags(opcode);
                                        setsbc(GETADDR(RN), templ, GETADDR(RN) - (templ + templ2));
                                        armregs[RD] = GETADDR(RN) - (templ + templ2);
                                }
                                break;
                                case 0x2E: /*RSC imm*/
                                templ2 = (CFSET) ? 0 : 1;
                                if (RD == 15)
                                {
                                        templ = rotate_noflags(opcode);
                                        LOAD_R15(templ - (GETADDR(RN) + templ2));
                                }
                                else
                                {
                                        templ = rotate_noflags(opcode);
                                        armregs[RD] = templ - (GETADDR(RN) + templ2);
                                }
                                break;
                                case 0x2F: /*RSCS imm*/
                                templ2 = (CFSET) ? 0 : 1;
                                if (RD == 15)
                                {
                                        templ = rotate_noflags(opcode);
                                        LOAD_R15_S(templ - (GETADDR(RN) + templ2));
                                }
                                else
                                {
                                        templ = rotate_noflags(opcode);
                                        setsbc(templ, GETADDR(RN), templ - (GETADDR(RN) + templ2));
                                        armregs[RD] = templ - (GETADDR(RN) + templ2);
                                }
                                break;

                                case 0x31: /*TST imm*/
                                if (RD == 15)
                                {
                                        if (armregs[15] & 3)
                                        {
                                                templ = armregs[15] & 0x3FFFFFC;
                                                armregs[15] = ((GETADDR(RN) & rotate_noflags(opcode)) & 0xFC000003) | templ;
                                        }
                                        else
                                        {
                                                templ = armregs[15] & 0x0FFFFFFF;
                                                armregs[15] = ((GETADDR(RN) & rotate_noflags(opcode)) & 0xF0000000) | templ;
                                        }
                                }
                                else
                                {
                                        setzn(GETADDR(RN) & rotate(opcode));
                                }
                                break;

                                case 0x32:
                                break;

                                case 0x33: /*TEQ imm*/
                                if (RD == 15)
                                {
                                        opcode &= ~0x100000;
                                        if (armregs[15] & 3)
                                        {
                                                templ = armregs[15] & 0x3FFFFFC;
                                                armregs[15] = ((GETADDR(RN) ^ rotate_noflags(opcode)) & 0xFC000003) | templ;
                                        }
                                        else
                                        {
                                                templ = armregs[15] & 0x0FFFFFFF;
                                                armregs[15] = ((GETADDR(RN) ^ rotate_noflags(opcode)) & 0xF0000000) | templ;
                                        }
                                }
                                else
                                {
                                        setzn(GETADDR(RN) ^ rotate(opcode));
                                }
                                break;

                                case 0x34:
                                break;
                                case 0x35: /*CMP imm*/
                                if (RD == 15)
                                {
                                        if (armregs[15] & 3)
                                        {
                                                templ = armregs[15] & 0x3FFFFFC;
                                                armregs[15] = ((GETADDR(RN) - rotate_noflags(opcode)) & 0xFC000003) | templ;
                                        }
                                        else
                                        {
                                                templ = armregs[15] & 0x0FFFFFFF;
                                                armregs[15] = ((GETADDR(RN) - rotate_noflags(opcode)) & 0xF0000000) | templ;
                                        }
                                }
                                else
                                {
                                        setsub(GETADDR(RN), rotate_noflags(opcode), GETADDR(RN) - rotate_noflags(opcode));
                                }
                                break;

                                case 0x37: /*CMN imm*/
                                if (RD == 15)
                                {
                                        if (armregs[15] & 3)
                                        {
                                                templ = armregs[15] & 0x3FFFFFC;
                                                armregs[15] = ((GETADDR(RN) + rotate_noflags(opcode)) & 0xFC000003) | templ;
                                        }
                                        else
                                        {
                                                templ = armregs[15] & 0x0FFFFFFF;
                                                armregs[15] = ((GETADDR(RN) + rotate_noflags(opcode)) & 0xF0000000) | templ;
                                        }
                                }
                                else
                                {
                                        setadd(GETADDR(RN), rotate_noflags(opcode), GETADDR(RN) + rotate_noflags(opcode));
                                }
                                break;

                                case 0x38: /*ORR imm*/
                                if (RD == 15)
                                {
                                        templ = rotate_noflags(opcode);
                                        LOAD_R15(GETADDR(RN) | templ);
                                }
                                else
                                {
                                        templ = rotate_noflags(opcode);
                                        armregs[RD] = GETADDR(RN) | templ;
                                }
                                break;
                                case 0x39: /*ORRS imm*/
                                if (RD == 15)
                                {
                                        templ = rotate_noflags(opcode);
                                        LOAD_R15_S(GETADDR(RN) | templ);
                                }
                                else
                                {
                                        templ = rotate(opcode);
                                        armregs[RD] = GETADDR(RN) | templ;
                                        setzn(armregs[RD]);
                                }
                                break;

                                case 0x3A: /*MOV imm*/
                                if (RD == 15)
                                        LOAD_R15(rotate_noflags(opcode));
                                else
                                        armregs[RD] = rotate_noflags(opcode);
                                break;
                                case 0x3B: /*MOVS imm*/
                                if (RD == 15)
                                        LOAD_R15_S(rotate_noflags(opcode));
                                else
                                {
                                        armregs[RD] = rotate(opcode);
                                        setzn(armregs[RD]);
                                }
                                break;

                                case 0x3C: /*BIC imm*/
                                if (RD == 15)
                                {
                                        templ = rotate_noflags(opcode);
                                        LOAD_R15(GETADDR(RN) & ~templ);
                                }
                                else
                                {
                                        templ = rotate_noflags(opcode);
                                        armregs[RD] = GETADDR(RN) & ~templ;
                                }
                                break;
                                case 0x3D: /*BICS imm*/
                                if (RD == 15)
                                {
                                        templ = rotate_noflags(opcode);
                                        LOAD_R15_S(GETADDR(RN) & ~templ);
                                }
                                else
                                {
                                        templ = rotate(opcode);
                                        armregs[RD] = GETADDR(RN) & ~templ;
                                        setzn(armregs[RD]);
                                }
                                break;

                                case 0x3E: /*MVN imm*/
                                if (RD == 15)
                                        LOAD_R15(~rotate_noflags(opcode));
                                else
                                        armregs[RD] = ~rotate_noflags(opcode);
                                break;
                                case 0x3F: /*MVNS imm*/
                                if (RD == 15)
                                        LOAD_R15_S(~rotate_noflags(opcode));
                                else
                                {
                                        armregs[RD] = ~rotate(opcode);
                                        setzn(armregs[RD]);
                                }
                                break;

                                case 0x47: case 0x4F: /*LDRBT*/
                                addr = GETADDR(RN);
                                addr2 = opcode & 0xFFF;
                                if (!(opcode & 0x800000))
                                        addr2 = -addr2;
                                if (opcode & 0x1000000)
                                        addr += addr2;
                                CHECK_ADDR_EXCEPTION(addr);
                                templ = memmode;
                                memmode = 0;
                                templ2 = readmemb(addr);
                                memmode = templ;
                                if (databort)
                                        break;
                                cache_read_timing(addr, 1);
                                LOADREG(RD, templ2);
                                if (!(opcode & 0x1000000))
                                {
                                        addr += addr2;
                                        armregs[RN] = addr;
                                }
                                else if (opcode&0x200000)
                                        armregs[RN]=addr;
                                if (memc_is_memc1)                   { CLOCK_N(PC); tsc += cyc_n; }            /* + 1N*/
                                else if (cp15_cacheon || (PC & 0xc)) { CLOCK_I();   tsc += cyc_i; }            /* + 1I*/
                                break;

                                case 0x41: case 0x49: case 0x61: case 0x69: /*LDR Rd,[Rn],offset*/
                                addr = GETADDR(RN);
                                if (opcode & 0x2000000)
                                {
                                        if (opcode & 0x10) /*Shift by register*/
                                        {
                                                EXCEPTION_UNDEFINED();
                                                break;
                                        }
                                        addr2 = shift_mem(opcode);
                                }
                                else
                                        addr2 = opcode & 0xFFF;
                                CHECK_ADDR_EXCEPTION(addr);
                                templ2 = ldrresult(readmeml(addr), addr);
                                if (databort)
                                        break;
                                cache_read_timing(addr, 1);
                                LOADREG(RD, templ2);
                                if (opcode & 0x800000)
                                        armregs[RN] += addr2;
                                else
                                        armregs[RN] -= addr2;
                                if (memc_is_memc1)                   { CLOCK_N(PC); tsc += cyc_n; }            /* + 1N*/
                                else if (cp15_cacheon || (PC & 0xc)) { CLOCK_I();   tsc += cyc_i; }            /* + 1I*/
                                break;
                                case 0x43: case 0x4B: case 0x63: case 0x6B: /*LDRT Rd,[Rn],offset*/
                                addr = GETADDR(RN);
                                if (opcode & 0x2000000)
                                {
                                        if (opcode & 0x10) /*Shift by register*/
                                        {
                                                EXCEPTION_UNDEFINED();
                                                break;
                                        }
                                        addr2 = shift_mem(opcode);
                                }
                                else
                                        addr2 = opcode & 0xFFF;
                                CHECK_ADDR_EXCEPTION(addr);
                                templ = memmode;
                                memmode = 0;
                                templ2 = ldrresult(readmeml(addr), addr);
                                memmode = templ;
                                if (databort)
                                        break;
                                cache_read_timing(addr, 1);
                                LOADREG(RD, templ2);
                                if (opcode & 0x800000)
                                        armregs[RN] += addr2;
                                else
                                        armregs[RN] -= addr2;
                                if (memc_is_memc1)                   { CLOCK_N(PC); tsc += cyc_n; }            /* + 1N*/
                                else if (cp15_cacheon || (PC & 0xc)) { CLOCK_I();   tsc += cyc_i; }            /* + 1I*/
                                break;

                                case 0x40: case 0x48: case 0x60: case 0x68: /*STR Rd,[Rn],offset*/
                                addr = GETADDR(RN);
                                if (opcode & 0x2000000)
                                {
                                        if (opcode & 0x10) /*Shift by register*/
                                        {
                                                EXCEPTION_UNDEFINED();
                                                break;
                                        }
                                        addr2 = shift_mem(opcode);
                                }
                                else
                                        addr2 = opcode & 0xFFF;
                                CHECK_ADDR_EXCEPTION(addr);
                                if (RD == 15) { writememl(addr, armregs[RD] + 4); }
                                else          { writememl(addr, armregs[RD]); }
                                if (databort)
                                        break;
                                cache_write_timing(addr, 1);
                                if (opcode & 0x800000)
                                        armregs[RN] += addr2;
                                else
                                        armregs[RN] -= addr2;
                                if (!cp15_cacheon && (PC & 0xc)) { CLOCK_I(); tsc += cyc_i; }
                                break;
                                case 0x42: case 0x4A: case 0x62: case 0x6A: /*STRT Rd,[Rn],offset*/
                                addr = GETADDR(RN);
                                if (opcode & 0x2000000)
                                {
                                        if (opcode & 0x10) /*Shift by register*/
                                        {
                                                EXCEPTION_UNDEFINED();
                                                break;
                                        }
                                        addr2 = shift_mem(opcode);
                                }
                                else
                                        addr2 = opcode & 0xFFF;
                                CHECK_ADDR_EXCEPTION(addr);
                                templ = memmode;
                                memmode = 0;
                                if (RD == 15) { writememl(addr,armregs[RD]+4); }
                                else          { writememl(addr,armregs[RD]); }
                                memmode = templ;
                                if (databort)
                                        break;
                                cache_write_timing(addr, 1);
                                if (opcode & 0x800000)
                                        armregs[RN] += addr2;
                                else
                                        armregs[RN] -= addr2;
                                if (!cp15_cacheon && (PC & 0xc)) { CLOCK_I(); tsc += cyc_i; }
                                break;
                                case 0x50: case 0x58: case 0x70: case 0x78: /*STR Rd,[Rn,offset]*/
                                case 0x52: case 0x5A: case 0x72: case 0x7A: /*STR Rd,[Rn,offset]!*/
                                if (opcode & 0x2000000)
                                {
                                        if (opcode & 0x10) /*Shift by register*/
                                        {
                                                EXCEPTION_UNDEFINED();
                                                break;
                                        }
                                        addr2 = shift_mem(opcode);
                                }
                                else
                                        addr2 = opcode & 0xFFF;
                                if (opcode & 0x800000)
                                        addr = GETADDR(RN) + addr2;
                                else
                                        addr = GETADDR(RN) - addr2;
                                CHECK_ADDR_EXCEPTION(addr);
                                if (RD==15) { writememl(addr,armregs[RD]+4); }
                                else        { writememl(addr,armregs[RD]); }
                                if (databort)
                                        break;
                                cache_write_timing(addr, 1);
                                if (opcode & 0x200000)
                                        armregs[RN] = addr;
                                if (!cp15_cacheon && (PC & 0xc)) { CLOCK_I(); tsc += cyc_i; }
                                break;

                                case 0x64: case 0x6C: /*STRB Rd,[Rn],offset*/
                                if (opcode & 0x10) /*Shift by register*/
                                {
                                        EXCEPTION_UNDEFINED();
                                        break;
                                }
                                case 0x44: case 0x4C:
                                addr = GETADDR(RN);
                                if (opcode & 0x2000000)
                                        addr2 = shift_mem(opcode);
                                else
                                        addr2 = opcode & 0xFFF;
                                CHECK_ADDR_EXCEPTION(addr);
                                writememb(addr, armregs[RD]);
                                if (databort)
                                        break;
                                cache_write_timing(addr, 1);
                                if (opcode & 0x800000)
                                        armregs[RN] += addr2;
                                else
                                        armregs[RN] -= addr2;
                                if (!cp15_cacheon && (PC & 0xc)) { CLOCK_I(); tsc += cyc_i; }
                                break;

                                case 0x66: case 0x6E: /*STRBT Rd,[Rn],offset*/
                                if (opcode & 0x10) /*Shift by register*/
                                {
                                        EXCEPTION_UNDEFINED();
                                        break;
                                }
                                case 0x46: case 0x4E:
                                addr = GETADDR(RN);
                                if (opcode & 0x2000000)
                                        addr2 = shift_mem(opcode);
                                else
                                        addr2 = opcode & 0xFFF;
                                CHECK_ADDR_EXCEPTION(addr);
                                writememb(addr, armregs[RD]);
                                templ = memmode;
                                memmode = 0;
                                if (databort)
                                        break;
                                cache_write_timing(addr, 1);
                                memmode = templ;
                                if (opcode & 0x800000)
                                        armregs[RN] += addr2;
                                else
                                        armregs[RN] -= addr2;
                                if (!cp15_cacheon && (PC & 0xc)) { CLOCK_I(); tsc += cyc_i; }
                                break;
                                
                                case 0x74: case 0x76: case 0x7c: case 0x7e: /*STRB Rd,[Rn,offset]*/
                                if (opcode & 0x10) /*Shift by register*/
                                {
                                        EXCEPTION_UNDEFINED();
                                        break;
                                }
                                case 0x54: case 0x56: case 0x5C: case 0x5E: /*STRB Rd,[Rn,offset]!*/
                                if (opcode & 0x2000000)
                                        addr2 = shift_mem(opcode);
                                else
                                        addr2 = opcode & 0xFFF;
                                if (opcode & 0x800000)
                                        addr = GETADDR(RN) + addr2;
                                else
                                        addr = GETADDR(RN) - addr2;
                                CHECK_ADDR_EXCEPTION(addr);
                                writememb(addr, armregs[RD]);
                                if (databort)
                                        break;
                                cache_write_timing(addr, 1);
                                if (opcode & 0x200000)
                                        armregs[RN] = addr;
                                if (!cp15_cacheon && (PC & 0xc)) { CLOCK_I(); tsc += cyc_i; }
                                break;


//                                        case 0x41: case 0x49: /*LDR*/
//                                        case 0x61: case 0x69:
                                case 0x71: case 0x73: case 0x79: case 0x7B:
                                if (opcode & 0x10)
                                {
                                        EXCEPTION_UNDEFINED();
                                        break;
                                }
                                case 0x51: case 0x53: case 0x59: case 0x5B:
                                addr = GETADDR(RN);
                                if (opcode & 0x2000000)
                                        addr2 = shift_mem(opcode);
                                else
                                        addr2 = opcode & 0xFFF;
                                if (!(opcode & 0x800000))
                                        addr2 = -addr2;
                                if (opcode & 0x1000000)
                                        addr += addr2;
                                CHECK_ADDR_EXCEPTION(addr);
                                templ = readmeml(addr);
                                templ = ldrresult(templ, addr);
                                if (databort)
                                        break;
                                cache_read_timing(addr, 1);
                                if (!(opcode & 0x1000000))
                                {
                                        addr += addr2;
                                        armregs[RN] = addr;
                                }
                                else if (opcode & 0x200000)
                                        armregs[RN] = addr;
                                LOADREG(RD, templ);
                                if (memc_is_memc1)                   { CLOCK_N(PC); tsc += cyc_n; }            /* + 1N*/
                                else if (cp15_cacheon || (PC & 0xc)) { CLOCK_I();   tsc += cyc_i; }            /* + 1I*/
                                break;

                                case 0x65: case 0x6D:
                                case 0x75: case 0x77: case 0x7D: case 0x7F:
                                if (opcode & 0x10)
                                {
                                        EXCEPTION_UNDEFINED();
                                        break;
                                }
                                case 0x45: case 0x4D: /*LDRB*/
                                case 0x55: case 0x57: case 0x5D: case 0x5F:
                                addr = GETADDR(RN);
                                if (opcode & 0x2000000)
                                        addr2 = shift_mem(opcode);
                                else
                                        addr2 = opcode & 0xFFF;
                                if (!(opcode&0x800000))
                                        addr2=-addr2;
                                if (opcode&0x1000000)
                                        addr+=addr2;
                                CHECK_ADDR_EXCEPTION(addr);
                                templ = readmemb(addr);
                                if (databort)
                                        break;
                                cache_read_timing(addr, 1);
                                if (!(opcode & 0x1000000))
                                {
                                        addr += addr2;
                                        armregs[RN] = addr;
                                }
                                else if (opcode & 0x200000)
                                        armregs[RN] = addr;
                                armregs[RD] = templ;
                                if (memc_is_memc1)                   { CLOCK_N(PC); tsc += cyc_n; }            /* + 1N*/
                                else if (cp15_cacheon || (PC & 0xc)) { CLOCK_I();   tsc += cyc_i; }            /* + 1I*/
                                break;

#define STMfirst()      mask=1; \
                        CHECK_ADDR_EXCEPTION(addr); \
                        for (c = 0; c < 15; c++) \
                        { \
                                if (opcode & mask) \
                                { \
                                        if (c == 15) { writememl(addr, armregs[c] + 4); } \
                                        else         { writememl(addr, armregs[c]); } \
                                        cache_write_timing(addr, 1); \
                                        addr += 4; \
                                        break; \
                                } \
                                mask <<= 1; \
                        } \
                        mask <<= 1; c++;
                        
#define STMall()        for (; c < 15; c++) \
                        { \
                                if (opcode & mask) \
                                { \
                                        writememl(addr, armregs[c]); \
                                        cache_write_timing(addr, !(addr & 0xc)); \
                                        addr += 4; \
                                } \
                                mask <<= 1; \
                        } \
                        if (opcode & 0x8000) \
                        { \
                                writememl(addr, armregs[15] + 4); \
                                cache_write_timing(addr, !(addr & 0xc)); \
                        }

#define STMfirstS()     mask = 1; \
                        CHECK_ADDR_EXCEPTION(addr); \
                        for (c = 0; c < 15; c++) \
                        { \
                                if (opcode & mask) \
                                { \
                                        if (c == 15) { writememl(addr, armregs[c] + 4); } \
                                        else         { writememl(addr, *usrregs[c]); } \
                                        cache_write_timing(addr, 1); \
                                        addr += 4; \
                                        break; \
                                } \
                                mask <<= 1; \
                        } \
                        mask <<= 1; c++;

#define STMallS()       for (; c < 15; c++) \
                        { \
                                if (opcode & mask) \
                                { \
                                        writememl(addr, *usrregs[c]); \
                                        cache_write_timing(addr, !(addr & 0xc)); \
                                        addr += 4; \
                                } \
                                mask <<= 1; \
                        } \
                        if (opcode & 0x8000) \
                        { \
                                writememl(addr, armregs[15] + 4); \
                                cache_write_timing(addr, !(addr & 0xc)); \
                        }

#define LDMall()        mask = 1; \
                        CHECK_ADDR_EXCEPTION(addr); \
                        for (c = 0; c < 15; c++) \
                        { \
                                if (opcode & mask) \
                                { \
                                        templ = readmeml(addr); if (!databort) armregs[c] = templ; \
                                        cache_read_timing(addr, !(opcode & (mask - 1)) || !(addr & 0xc)); \
                                        addr+=4; \
                                } \
                                mask<<=1; \
                        } \
                        if (opcode & 0x8000) \
                        { \
                                templ = readmeml(addr); \
                        	cache_read_timing(addr, !(addr & 0xc)); \
                                addr += 4; \
                                if (!databort) armregs[15] = (armregs[15] & 0xFC000003) | ((templ+4) & 0x3FFFFFC); \
                                refillpipeline(); \
                        }

#define LDMallS()       mask = 1; \
                        CHECK_ADDR_EXCEPTION(addr); \
                        if (opcode & 0x8000) \
                        { \
                                for (c = 0; c < 15; c++) \
                                { \
                                        if (opcode & mask) \
                                        { \
                                                templ = readmeml(addr); if (!databort) armregs[c] = templ; \
		                        	cache_read_timing(addr, !(opcode & (mask - 1)) || !(addr & 0xc)); \
                                                addr += 4; \
                                        } \
                                        mask <<= 1; \
                                } \
                                templ = readmeml(addr); \
                        	cache_read_timing(addr, !(opcode & (mask - 1)) || !(addr & 0xc)); \
                                addr += 4; \
                                if (!databort) \
                                { \
                                        if (armregs[15] & 3) armregs[15] = (templ + 4); \
                                        else                 armregs[15] = (armregs[15] & 0x0C000003) | ((templ + 4) & 0xF3FFFFFC); \
                                } \
                                refillpipeline(); \
                        } \
                        else \
                        { \
                                for (c = 0; c < 15; c++) \
                                { \
                                        if (opcode & mask) \
                                        { \
                                                templ = readmeml(addr); if (!databort) *usrregs[c] = templ; \
                                                addr += 4; \
                                        } \
                                        mask <<= 1; \
                                } \
                        }

                                case 0x80: /*STMDA*/
                                case 0x82: /*STMDA !*/
                                case 0x90: /*STMDB*/
                                case 0x92: /*STMDB !*/
                                addr = armregs[RN] - countbits(opcode & 0xFFFF);
                                if (!(opcode & 0x1000000))
                                        addr += 4;
                                STMfirst();
                                if ((opcode & 0x200000) && (RN != 15))
                                        armregs[RN] -= countbits(opcode & 0xFFFF);
                                STMall()
                                if (PC & 0xc) { CLOCK_I(); tsc += cyc_i; }
                                break;
                                case 0x88: /*STMIA*/
                                case 0x8A: /*STMIA !*/
                                case 0x98: /*STMIB*/
                                case 0x9A: /*STMIB !*/
                                addr = armregs[RN];
                                if (opcode & 0x1000000)
                                        addr += 4;
                                STMfirst();
                                if ((opcode & 0x200000) && (RN != 15))
                                        armregs[RN] += countbits(opcode & 0xFFFF);
                                STMall();
                                if (PC & 0xc) { CLOCK_I(); tsc += cyc_i; }
                                break;
                                case 0x84: /*STMDA ^*/
                                case 0x86: /*STMDA ^!*/
                                case 0x94: /*STMDB ^*/
                                case 0x96: /*STMDB ^!*/
                                addr = armregs[RN] - countbits(opcode & 0xFFFF);
                                if (!(opcode & 0x1000000))
                                        addr += 4;
                                STMfirstS();
                                if ((opcode & 0x200000) && (RN != 15))
                                        armregs[RN] -= countbits(opcode & 0xFFFF);
                                STMallS()
                                if (PC & 0xc) { CLOCK_I(); tsc += cyc_i; }
                                break;
                                case 0x8C: /*STMIA ^*/
                                case 0x8E: /*STMIA ^!*/
                                case 0x9C: /*STMIB ^*/
                                case 0x9E: /*STMIB ^!*/
                                addr = armregs[RN];
                                if (opcode & 0x1000000)
                                        addr += 4;
                                STMfirstS();
                                if ((opcode & 0x200000) && (RN != 15))
                                        armregs[RN]+=countbits(opcode&0xFFFF);
                                STMallS();
                                if (PC & 0xc) { CLOCK_I(); tsc += cyc_i; }
                                break;
                                
                                case 0x81: /*LDMDA*/
                                case 0x83: /*LDMDA !*/
                                case 0x91: /*LDMDB*/
                                case 0x93: /*LDMDB !*/
                                addr = armregs[RN] - countbits(opcode & 0xFFFF);
//                                        rpclog("LDMDB %08X\n",addr);
                                if (!(opcode & 0x1000000))
                                        addr += 4;
                                if ((opcode & 0x200000) && (RN != 15))
                                        armregs[RN] -= countbits(opcode & 0xFFFF);
                                LDMall();
				if (memc_is_memc1)
				{
					/*MEMC1 - repeat last cycle. */
					if (!((addr - 4) & 0xc))
					{
						CLOCK_N(PC);
						tsc += cyc_n;
					}
					else
					{
						CLOCK_S(PC);
						tsc += cyc_s;
					}
				}
				else
				{
                                	if (PC & 0xc) { CLOCK_I(); tsc += cyc_i; }                        /* + 1I*/
				}
                                break;
                                case 0x89: /*LDMIA*/
                                case 0x8B: /*LDMIA !*/
                                case 0x99: /*LDMIB*/
                                case 0x9B: /*LDMIB !*/
                                addr = armregs[RN];
                                if (opcode & 0x1000000)
                                        addr+=4;
                                if ((opcode & 0x200000) && (RN != 15))
                                        armregs[RN]+=countbits(opcode & 0xFFFF);
                                LDMall();
				if (memc_is_memc1)
				{
					/*MEMC1 - repeat last cycle. */
					if (!((addr - 4) & 0xc))
					{
						CLOCK_N(PC);
						tsc += cyc_n;
					}
					else
					{
						CLOCK_S(PC);
						tsc += cyc_s;
					}
				}
				else
				{
                                	if (PC & 0xc) { CLOCK_I(); tsc += cyc_i; }                        /* + 1I*/
				}
                                break;
                                case 0x85: /*LDMDA ^*/
                                case 0x87: /*LDMDA ^!*/
                                case 0x95: /*LDMDB ^*/
                                case 0x97: /*LDMDB ^!*/
                                addr = armregs[RN] - countbits(opcode & 0xFFFF);
                                if (!(opcode & 0x1000000))
                                        addr += 4;
                                if ((opcode & 0x200000) && (RN != 15))
                                        armregs[RN] -= countbits(opcode & 0xFFFF);
                                LDMallS();
				if (memc_is_memc1)
				{
					/*MEMC1 - repeat last cycle. */
					if (!((addr - 4) & 0xc))
					{
						CLOCK_N(PC);
						tsc += cyc_n;
					}
					else
					{
						CLOCK_S(PC);
						tsc += cyc_s;
					}
				}
				else
				{
                                	if (PC & 0xc) { CLOCK_I(); tsc += cyc_i; }                        /* + 1I*/
				}
                                break;
                                case 0x8D: /*LDMIA ^*/
                                case 0x8F: /*LDMIA ^!*/
                                case 0x9D: /*LDMIB ^*/
                                case 0x9F: /*LDMIB ^!*/
                                addr = armregs[RN];
                                if (opcode & 0x1000000)
                                        addr += 4;
                                if ((opcode & 0x200000) && (RN != 15))
                                        armregs[RN]+=countbits(opcode&0xFFFF);
                                LDMallS();
				if (memc_is_memc1)
				{
					/*MEMC1 - repeat last cycle. */
					if (!((addr - 4) & 0xc))
					{
						CLOCK_N(PC);
						tsc += cyc_n;
					}
					else
					{
						CLOCK_S(PC);
						tsc += cyc_s;
					}
				}
				else
				{
                                	if (PC & 0xc) { CLOCK_I(); tsc += cyc_i; }                        /* + 1I*/
				}
                                break;

                                case 0xB0: case 0xB1: case 0xB2: case 0xB3: /*BL*/
                                case 0xB4: case 0xB5: case 0xB6: case 0xB7:
                                case 0xB8: case 0xB9: case 0xBA: case 0xBB:
                                case 0xBC: case 0xBD: case 0xBE: case 0xBF:
                                templ = (opcode & 0xFFFFFF) << 2;
                                armregs[14] = armregs[15] - 4;
                                armregs[15] = ((armregs[15] + templ + 4) & 0x3FFFFFC) | (armregs[15] & 0xFC000003);
                                refillpipeline();
                                break;

                                case 0xA0: case 0xA1: case 0xA2: case 0xA3: /*B*/
                                case 0xA4: case 0xA5: case 0xA6: case 0xA7:
                                case 0xA8: case 0xA9: case 0xAA: case 0xAB:
                                case 0xAC: case 0xAD: case 0xAE: case 0xAF:
                                templ = (opcode & 0xFFFFFF) << 2;
                                armregs[15] = ((armregs[15] + templ + 4) & 0x3FFFFFC) | (armregs[15] & 0xFC000003);
                                refillpipeline();
                                break;

                                case 0xE0: case 0xE2: case 0xE4: case 0xE6: /*MCR*/
                                case 0xE8: case 0xEA: case 0xEC: case 0xEE:
                                if (fpaena && MULRS == 1)
                                {
                                        if (fpaopcode(opcode))
                                                EXCEPTION_UNDEFINED();
                                }
                                else if (MULRS == 15 && (opcode & 0x10) && arm_has_cp15)
                                {
                                        writecp15(RN,armregs[RD]);
                                }
                                else
                                        EXCEPTION_UNDEFINED();
                                break;

                                case 0xE1: case 0xE3: case 0xE5: case 0xE7: /*MRC*/
                                case 0xE9: case 0xEB: case 0xED: case 0xEF:
                                if (fpaena && MULRS == 1)
                                {
                                        if (fpaopcode(opcode))
                                                EXCEPTION_UNDEFINED();
                                }
                                else if (MULRS == 15 && (opcode & 0x10) && arm_has_cp15)
                                {
                                        if (RD == 15) armregs[RD] = (armregs[RD] & 0x3FFFFFC) | (readcp15(RN) & 0xFC000003);
                                        else          armregs[RD] = readcp15(RN);
                                }
                                else
                                        EXCEPTION_UNDEFINED();
                                break;
                                
                                case 0xC0: case 0xC1: case 0xC2: case 0xC3: /*Co-pro*/
                                case 0xC4: case 0xC5: case 0xC6: case 0xC7:
                                case 0xC8: case 0xC9: case 0xCA: case 0xCB:
                                case 0xCC: case 0xCD: case 0xCE: case 0xCF:
                                case 0xD0: case 0xD1: case 0xD2: case 0xD3:
                                case 0xD4: case 0xD5: case 0xD6: case 0xD7:
                                case 0xD8: case 0xD9: case 0xDA: case 0xDB:
                                case 0xDC: case 0xDD: case 0xDE: case 0xDF:
                                if (((opcode & 0xF00) == 0x100 || (opcode & 0xF00) == 0x200) && fpaena)
                                {
                                        if (fpaopcode(opcode))
                                                EXCEPTION_UNDEFINED();
                                }
                                else
                                        EXCEPTION_UNDEFINED();
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
                                        EXCEPTION_SWI();
                                break;

                                default:
                                rpclog("Illegal instruction %08X %07X\n",opcode, PC);
                                EXCEPTION_UNDEFINED();
                        }
                }
                else if (!prefabort)
                {
			CLOCK_I();
                	tsc += cyc_i;
		}
                if (databort|armirq|prefabort)
                {
                        if (prefabort)       /*Prefetch abort*/
                        {
                                prefabort = 0;
                                EXCEPTION_PREF_ABORT();
                        }
                        else if (databort == 1)     /*Data abort*/
                        {
                                databort = 0;
                                EXCEPTION_DATA_ABORT();
                        }
                        else if (databort == 2) /*Address Exception*/
                        {
                                databort = 0;
                                EXCEPTION_ADDRESS();
                        }
                        else if ((armirq&2) && !(armregs[15]&0x4000000)) /*FIQ*/
                        {
//                                rpclog("FIQ %02X %i\n",ioc.fiq&ioc.mskf, 0);
                                EXCEPTION_FIQ();
                        }
                        else if ((armirq&1) && !(armregs[15]&0x8000000)) /*IRQ*/
                        {
//                                rpclog("IRQ %02X %02X\n",ioc.irqa&ioc.mska,ioc.irqb&ioc.mskb);
                                EXCEPTION_IRQ();
                        }
                }
                prefabort = prefabort_next;
                armirq = irq;
                armregs[15] += 4;
                if ((armregs[15] & 3) != mode)
                        updatemode(armregs[15] & 3);

                if (output)
                {
                        rpclog("%05i : %07X %08X %08X %08X %08X %08X %08X %08X %08X",ins,PC-8,armregs[0],armregs[1],armregs[2],armregs[3],armregs[4],armregs[5],armregs[6],armregs[7]);
                        rpclog("  %08X %08X %08X %08X %08X %08X %08X %08X  %08X  %02X %02X %02X  %02X %02X %02X  %i %i %i %X %i\n",armregs[8],armregs[9],armregs[10],armregs[11],armregs[12],armregs[13],armregs[14],armregs[15],opcode,ioc.mska,ioc.mskb,ioc.mskf,ioc.irqa,ioc.irqb,ioc.fiq,  fdc_indexcount, 0, motoron, fdc_indexpulse, motorspin);
                        ins++;

                        if (timetolive)
                        {
                                timetolive--;
                                if (!timetolive)
                                {
                                        output=0;
//                                        dumpregs();
//                                        exit(-1);
                                }
                        }
                }

                inscount++;
                ins++;

		if (TIMER_VAL_LESS_THAN_VAL(timer_target, tsc >> 10))
			timer_process();

                cyc=((int)tsc-oldcyc) >> 10; /*Number of clock ticks executed*/
                clock_ticks_executed += cyc;
                total_cycles -= ((int)tsc-oldcyc);

        }
        LOG_EVENT_LOOP("execarm() finished; clock ticks=%d, and called pollline() %d times (should be ~160)\n",
                clock_ticks_executed, pollline_call_count);
}
