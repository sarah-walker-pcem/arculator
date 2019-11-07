/*Arculator 2.0 by Sarah Walker
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

#define cyc_i (1ull << 32)

uint32_t opcode;
static uint32_t opcode2,opcode3;
static uint32_t *usrregs[16],userregs[16],superregs[16],fiqregs[16],irqregs[16];

int vidc_fetches = 0;

int databort;
int prefabort, prefabort_next;

/*Archimedes has three clock domains :
        FCLK - fast CPU clock
        MCLK - MEMC clock
        IOCLK - IOC clock

  IOCLK is always 8 MHz. MCLK is either 8 or 12 on unmodified machines.

  Synchronising to MCLK costs 1F + 2L cycles - presumably 1F for cache lookup,
  2L for sync.
  
  During a cache fetch, ARM3 will be clocked once the request word has been
  fetched. It will continue to be clocked for successive S or I cycles.
*/
enum
{
        DOMAIN_FCLK,
        DOMAIN_MCLK,
        DOMAIN_IOCLK
};
static int clock_domain;

/*Timestamp of when memory is next available*/
static uint64_t mem_available_ts;

static uint64_t refresh_ts;

/*Remaining read cycles in the current line fill. If non-zero then I and S cycles
  will be promoted to MCLK-based S cycles. N cycles will have to wait until
  mem_available_ts*/
static int pending_reads = 0;
static uint32_t cache_fill_addr;

/*DMA priorities :
        Video/Cursor
        Sound
        Refresh*/
/*Two cases :
        On N-cycle - run until no pending DMA requests. TSC will need to be updated and
                timers need to run
        On I-cycle - run until TSC. TSC should not be updated, timers do not need to run*/

enum
{
        DMA_REFRESH,
        DMA_SOUND,
        DMA_CURSOR,
        DMA_VIDEO
};
static void run_dma(int update_tsc)
{
        while (1)
        {
                uint64_t min_timer = refresh_ts;
                int dma_source = DMA_REFRESH;
                
                if (memc_dma_sound_req && TIMER_VAL_LESS_THAN_VAL_64(memc_dma_sound_req_ts, min_timer))
                {
                        min_timer = memc_dma_sound_req_ts;
                        dma_source = DMA_SOUND;
                }
                if (memc_videodma_enable && memc_dma_cursor_req && TIMER_VAL_LESS_THAN_VAL_64(memc_dma_cursor_req_ts, min_timer))
                {
                        min_timer = memc_dma_cursor_req_ts;
                        dma_source = DMA_CURSOR;
                }
                if (memc_videodma_enable && memc_dma_video_req && TIMER_VAL_LESS_THAN_VAL_64(memc_dma_video_req_ts, min_timer))
                {
                        min_timer = memc_dma_video_req_ts;
                        dma_source = DMA_VIDEO;
                }
                if (TIMER_VAL_LESS_THAN_VAL_64(min_timer, tsc))
                {
                        switch (dma_source)
                        {
                                case DMA_REFRESH:
//                                if (output) rpclog("Refresh DMA %i\n", mem_dorefresh);
                                if (mem_dorefresh)
                                {
                                        if (TIMER_VAL_LESS_THAN_VAL_64(refresh_ts, mem_available_ts))
                                                mem_available_ts += 2 * mem_spd_multi;
                                        else
                                                mem_available_ts = refresh_ts + 2 * mem_spd_multi;
                                }
                                refresh_ts += 32 * mem_spd_multi;
                                break;
                                case DMA_SOUND:
//                                if (output) rpclog("Sound DMA\n");
                                if (TIMER_VAL_LESS_THAN_VAL_64(memc_dma_sound_req_ts, mem_available_ts))
                                        mem_available_ts += 5 * mem_spd_multi;
                                else
                                        mem_available_ts = memc_dma_sound_req_ts + 5 * mem_spd_multi;
                                memc_dma_sound_req = 0;
                                break;
                                case DMA_CURSOR:
//                                if (output) rpclog("Cursor DMA\n");
                                if (TIMER_VAL_LESS_THAN_VAL_64(memc_dma_cursor_req_ts, mem_available_ts))
                                        mem_available_ts += 5 * mem_spd_multi;
                                else
                                        mem_available_ts = memc_dma_cursor_req_ts + 5 * mem_spd_multi;
                                memc_dma_cursor_req = 0;
                                break;
                                case DMA_VIDEO:
//                                if (output) rpclog("Video fetch %i\n", memc_dma_video_req);
                                if (TIMER_VAL_LESS_THAN_VAL_64(memc_dma_video_req_ts, mem_available_ts))
                                        mem_available_ts += memc_dma_video_req * 5 * mem_spd_multi;
                                else
                                        mem_available_ts = memc_dma_video_req_ts + memc_dma_video_req * 5 * mem_spd_multi;
                                if (memc_dma_video_req == 2)
                                {
                                        memc_dma_video_req_ts = memc_dma_video_req_start_ts;
                                        memc_dma_video_req = 1;
                                }
                                else
                                        memc_dma_video_req_ts += memc_dma_video_req_period;
                                break;
                        }
                        if (update_tsc && TIMER_VAL_LESS_THAN_VAL_64(tsc, mem_available_ts))
                        {
                                tsc = mem_available_ts;
                                if (TIMER_VAL_LESS_THAN_VAL(timer_target, tsc >> 32))
                                	timer_process();
                        }
                }
                else
                        break;
        }
}

static void sync_to_mclk(void)
{
        if (clock_domain != DOMAIN_MCLK)
        {
                uint64_t sync_cycles = mem_spd_multi - (tsc % mem_spd_multi);
                tsc += sync_cycles; /*Now synchronised to MCLK*/
                tsc += mem_spd_multi; /*Synchronising takes another L cycle according to the ARM3 datasheet*/
        }
        
        if (TIMER_VAL_LESS_THAN_VAL(timer_target, tsc >> 32))
        	timer_process();

        clock_domain = DOMAIN_MCLK;
        run_dma(1);
}
static void sync_to_fclk(void)
{
        if (clock_domain != DOMAIN_FCLK)
        {
                tsc = (tsc + 0x3ff) & ~0x3ff;

                clock_domain = DOMAIN_FCLK;
        }
}

static int last_cycle_length = 0;

static void CLOCK_N(uint32_t addr)
{
        tsc += mem_speed[(addr >> 12) & 0x3fff][1];
        last_cycle_length = mem_speed[(addr >> 12) & 0x3fff][1];
}

static void CLOCK_S(uint32_t addr)
{
        tsc += mem_speed[(addr >> 12) & 0x3fff][0];
        last_cycle_length = mem_speed[(addr >> 12) & 0x3fff][0];
}

static void CLOCK_I()
{
        if (pending_reads)
        {
                /*Cache fill is in progress. As CPU is currently synced to MCLK,
                  'promote' this cycle to an S-cycle*/
                pending_reads--;
                CLOCK_S(cache_fill_addr);
        }
        else
        {
//                if (output)
//                        rpclog(" CLOCK_I run_dma\n");
                run_dma(0);
                if (memc_is_memc1)
                        tsc += last_cycle_length;
                else
                        tsc += cyc_i;
        }
}

void arm_clock_i(int i_cycles)
{
        while (i_cycles--)
                CLOCK_I();
}

static void cache_line_fill(uint32_t addr, int byte_offset, int bit_offset)
{
        int set = (addr >> 4) & 3;

        if (arm3_cache_tag[set][arm3_slot] != TAG_INVALID)
        {
        	int old_bit_offset = (arm3_cache_tag[set][arm3_slot] >> 4) & 7;
        	int old_byte_offset = (arm3_cache_tag[set][arm3_slot] >> (4+3));

        	arm3_cache[old_byte_offset] &= ~(1 << old_bit_offset);
        }

        arm3_cache_tag[set][arm3_slot] = addr & ~0xf;
        arm3_cache[byte_offset] |= (1 << bit_offset);
        sync_to_mclk();

        mem_available_ts = tsc + mem_speed[(addr >> 12) & 0x3fff][1] + 3*mem_speed[(addr >> 12) & 0x3fff][0];
        
        /*ARM3 will start to clock the CPU again once the requested word has been
          read. So only 'charge' the emulated CPU up to that point, and promote
          subsequent cycles to memory cycles until the line fill has completed*/
        CLOCK_N(addr);
        if ((addr & 0xc) >= 4)
        {
                CLOCK_S(addr);
                if ((addr & 0xc) >= 8)
                {
                        CLOCK_S(addr);
                        if ((addr & 0xc) >= 0xc)
                                CLOCK_S(addr);
                }
        }
        pending_reads = 3 - ((addr & 0xc) >> 2);
        cache_fill_addr = addr & ~0xf;
        
        if (!((arm3_slot ^ (arm3_slot >> 1)) & 1))
        	arm3_slot |= 0x40;
        arm3_slot >>= 1;

//        rpclog("Cache line fill %08x. %i pending reads\n", addr, pending_reads);
}
enum
{
        PROMOTE_NONE = 0,
        PROMOTE_MERGE,
        PROMOTE_NOMERGE
};

static int cache_was_on = 0;
static int promote_fetch_to_n = PROMOTE_NONE;
void cache_read_timing(uint32_t addr, int is_n_cycle, int is_merged_fetch)
{
	int bit_offset = (addr >> 4) & 7;
	int byte_offset = ((addr & 0x3ffffff) >> (4+3));

        addr &= 0x3ffffff;
        
  //      if (output) rpclog("Read %c-cycle %07x\n", is_n_cycle?'N':'S', addr);
        if (is_n_cycle)
        {
                cache_was_on = 0;
                if (cp15_cacheon)
                {
//                        rpclog("N-cycle %08x %i %i\n", addr, clock_domain, pending_reads);
                        if (pending_reads)
                        {
#ifndef RELEASE_BUILD
                                if (clock_domain != DOMAIN_MCLK)
                                        fatal("N-cycle with pending reads - clock_domain != MCLK %07x\n", addr);
                                if (TIMER_VAL_LESS_THAN_VAL_64(mem_available_ts, tsc))
                                        fatal("N-cycle with pending reads - TS already expired? %016llx %016llx\n", mem_available_ts, tsc);
#endif
                                /*Complete pending line fill*/
                                pending_reads = 0;
                                tsc = mem_available_ts;
                        }

                        /*Always start N-cycle synced to FCLK - this is required for
                          cache lookup*/
                        sync_to_fclk();
                        
        		if (arm3_cache[byte_offset] & (1 << bit_offset))
        		{
                                cache_was_on = 1;
                                CLOCK_I(); /*Data is in cache*/
                        }
                        else if (!(arm3cp.cache & (1 << (addr >> 21))))
        		{
                                /*Data is uncacheable*/
                                sync_to_mclk();
                                CLOCK_N(addr);
                        }
	              	else
        		{
                                /*Data not in cache; perform cache fill*/
                                cache_line_fill(addr, byte_offset, bit_offset);
        		}
                }
                else
                {
                        sync_to_mclk();
                        CLOCK_N(addr);
                        mem_available_ts = tsc;
                        /*Merged fetch doesn't cause extended I-cycle on MEMC1*/
                        if (memc_is_memc1 && is_merged_fetch == PROMOTE_MERGE)
                                last_cycle_length = mem_speed[(addr >> 12) & 0x3fff][0];
                }
        }
        else
        {
                if (cp15_cacheon)
                {
//                        rpclog("S-cycle %08x %i %i\n", addr, clock_domain, pending_reads);
                        if (pending_reads)
                        {
#ifndef RELEASE_BUILD
                                if (clock_domain != DOMAIN_MCLK)
                                        fatal("S-cycle with pending reads - clock_domain != MCLK %07x\n", addr);
                                if (TIMER_VAL_LESS_THAN_VAL_64(mem_available_ts, tsc))
                                        fatal("S-cycle with pending reads - TS already expired? %016llx %016llx\n", mem_available_ts, tsc);
#endif
                                /*Cache line fill is in progress. Since the CPU
                                  being clocked and this is an S cycle, it must
                                  be the next word to be read*/
                                pending_reads--;
#ifndef RELEASE_BUILD
                                if (clock_domain != DOMAIN_MCLK)
                                        fatal("Data uncacheable - clock_domain != MCLK %07x\n", addr);
#endif
                                CLOCK_S(addr);
                        }
        		else if (cache_was_on && (arm3_cache[byte_offset] & (1 << bit_offset)))
        		{
#ifndef RELEASE_BUILD
                                if ((clock_domain != DOMAIN_FCLK) && ((addr & ~0xf) != cache_fill_addr))
                                        fatal("Data in cache - clock_domain != FCLK %08x\n", addr);
#endif
                                sync_to_fclk();
                                CLOCK_I(); /*Data is in cache*/
                        }
                        else if (!(arm3cp.cache & (1 << (addr >> 21))))
        		{
                                /*Data is uncacheable*/
#ifndef RELEASE_BUILD
                                if (clock_domain != DOMAIN_MCLK)
                                        fatal("Data uncacheable - clock_domain != MCLK %07x\n", addr);
#endif
                                CLOCK_S(addr);
                        }
                        else
                        {
                                cache_line_fill(addr, byte_offset, bit_offset);
//                                sync_to_fclk();
//                                fatal("Data not in cache for S cycle - should not be currently possible  %08x\n", addr);
                        }
                }
                else
                {
                        CLOCK_S(addr);
                        mem_available_ts = tsc;
                }
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
        addr &= 0x3ffffff;

//        if (output) rpclog("Write %c-cycle %08x\n", is_n_cycle ? 'N' : 'S', addr);
        if (pending_reads)
        {
#ifndef RELEASE_BUILD
                if (clock_domain != DOMAIN_MCLK)
                        fatal("Write cycle with pending reads - clock_domain != MCLK %07x\n", addr);
                if (TIMER_VAL_LESS_THAN_NE_VAL_64(mem_available_ts, tsc))
                        fatal("Write cycle with pending reads - TS already expired? %016llx %016llx\n", mem_available_ts, tsc);
#endif
                /*Complete pending line fill*/
                /*Note that ARM3 can go straight from a line fill to memory write
                  without resyncing to FCLK if the line fill is still incomplete
                  when the write is requested*/
                pending_reads = 0;
                tsc = mem_available_ts;
        }

	if (arm3cp.disrupt & (1 << (addr >> 21)))
		cache_flush();
        
	if (is_n_cycle)
	{
//                rpclog("Write N-cycle %08x\n", addr);
                sync_to_mclk();
		CLOCK_N(addr);
		mem_available_ts = tsc;
        }
	else
	{
//                rpclog("Write S-cycle %08x\n", addr);
#ifndef RELEASE_BUILD
                if (clock_domain != DOMAIN_MCLK)
                        fatal("Write S-cycle - not in MCLK %08x\n", addr);
#endif
		CLOCK_S(addr);
		mem_available_ts = tsc;
        }
}


/*Handle timing for last cycle of LDR/LDM/MUL/MLA/data processing intrucstions
  with register shift. On ARM2 machines this will be 'merged' with the following
  instruction fetch. On ARM3 the final cycle is just an I-cycle.*/
static void merge_timing(uint32_t addr)
{
        promote_fetch_to_n = PROMOTE_MERGE; /*Merge writeback with next fetch*/
        if (memc_is_memc1)
        {
                if ((addr & 0xc) == 0xc)
                {
                        /*MEMC1 does _not_ merge if A[2:3]=11, so clock an extra
                          I-cycle and push the next fetch to a non-merged N-cycle*/
                        CLOCK_I();
                        promote_fetch_to_n = PROMOTE_NOMERGE;
                }
                else
                        tsc += (last_cycle_length - cyc_i);
        }
        else if (cp15_cacheon)
                CLOCK_I();   /* + 1I*/
}


int arm_cpu_type;

int arm_cpu_speed, arm_mem_speed;
int arm_has_swp;
int arm_has_cp15;

int fpaena=0;

int irq;
uint8_t flaglookup[16][16];
static uint32_t rotatelookup[4096];
int timetolive=0;
int inscount;
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
        
        ins = 0;
	tsc = 0;
	mem_available_ts = 0;
	clock_domain = DOMAIN_MCLK;
	pending_reads = 0;
	promote_fetch_to_n = PROMOTE_NONE;
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
                merge_timing(PC+4);
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
                merge_timing(PC+4);
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
#ifndef RELEASE_BUILD
        if (opcode&0x10)
                fatal("Shift by register on memory shift!!! %08X\n",PC);
#endif
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

        cache_read_timing(PC-4, 1, 0);
        cache_read_timing(PC, !(PC & 0xc), 0);
}

void refillpipeline2()
{
        uint32_t templ,templ2,addr=PC-8;

        prefabort_next = 0;
        readmemfff(addr,opcode2);
        addr+=4;
        prefabort = prefabort_next;
        readmemfff(addr,opcode3);

        cache_read_timing(PC-8, 1, 0);
        cache_read_timing(PC-4, !((PC-4) & 0xc), 0);
}

/*Booth's algorithm implementation taken from Steve Furber's ARM System-On-Chip
  Architecture. This should replicate all the chaos caused by invalid register
  combinations, and is good enough to pass !SICK's 'not Virtual A5000' test.*/
static void opMUL(uint32_t rn)
{
        uint32_t rs = armregs[MULRS];
        int carry = 0;
        int shift = 0;
        int cycle_nr = 0;

        if (MULRD != 15)
                armregs[MULRD] = rn;
        while (((rs || carry) && shift < 32) || !cycle_nr)
        {
                int m = rs & 3;
                
                if (!carry)
                {
                        switch (m)
                        {
                                case 0:
                                carry = 0;
                                break;
                                case 1:
                                if (MULRD != 15)
                                        armregs[MULRD] += (armregs[MULRM] << shift);
                                carry = 0;
                                break;
                                case 2:
                                if (MULRD != 15)
                                        armregs[MULRD] -= (armregs[MULRM] << (shift+1));
                                carry = 1;
                                break;
                                case 3:
                                if (MULRD != 15)
                                        armregs[MULRD] -= (armregs[MULRM] << shift);
                                carry = 1;
                                break;
                        }
                }
                else
                {
                        switch (m)
                        {
                                case 0:
                                if (MULRD != 15)
                                        armregs[MULRD] += (armregs[MULRM] << shift);
                                carry = 0;
                                break;
                                case 1:
                                if (MULRD != 15)
                                        armregs[MULRD] += (armregs[MULRM] << (shift+1));
                                carry = 0;
                                break;
                                case 2:
                                if (MULRD != 15)
                                        armregs[MULRD] -= (armregs[MULRM] << shift);
                                carry = 1;
                                break;
                                case 3:
                                carry = 1;
                                break;
                        }
                }
                
                rs >>= 2;
                shift += 2;

                cycle_nr++;
        }
        if (!memc_is_memc1)
        {
                /*cycle_nr-1 I-cycles, plus a merged fetch*/
                arm_clock_i(cycle_nr-1);
                merge_timing(PC+4);
        }
        else
        {
                int c;
                
                /*1 (early) merged fetch, cycle_nr-1 N-cycles*/
                merge_timing(PC+4);
                for (c = 0; c < cycle_nr-1; c++)
                        CLOCK_N(PC+4);
        }
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

static int64_t total_cycles;

/*Execute ARM instructions for `cycs` clock ticks, typically 10 ms
  (cycs=80k for an 8MHz ARM2).*/
void execarm(int cycles_to_execute)
{
        uint32_t templ,templ2,mask,addr,addr2;
        int c;
        int cyc; /*Number of clock ticks executed in the last loop*/

        int clock_ticks_executed = 0;
        
        LOG_EVENT_LOOP("execarm(%d) total_cycles=%i\n", cycles_to_execute, total_cycles);

        total_cycles += (uint64_t)cycles_to_execute << 32;

        while (total_cycles>0)
        {
//                LOG_VIDC_TIMING("cycles (%d) += vidcgetcycs() (%d) --> %d (%d) to execute before pollline()\n",
//                        oldcyc, vidc_cycles_to_execute, 0, 0);
                uint64_t oldcyc = tsc;

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
                cache_read_timing(PC, ((PC & 0xc) && !promote_fetch_to_n) ? 0 : 1, promote_fetch_to_n);
                promote_fetch_to_n = PROMOTE_NONE;

                if (flaglookup[opcode >> 28][armregs[15] >> 28] && !prefabort)
                {
                        int first_access;
                
                        switch ((opcode >> 20) & 0xFF)
                        {
                                case 0x00: /*AND reg*/
                                if ((opcode&0x90) == 0x90) /*MUL*/
                                {
                                        opMUL(0);
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
                                if ((opcode & 0x90) == 0x90) /*MULS*/
                                {
                                        opMUL(0);
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
                                if ((opcode & 0x90) == 0x90) /*MLA*/
                                {
                                        opMUL(armregs[MULRN]);
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
                                if ((opcode & 0x90) == 0x90) /*MLAS*/
                                {
                                        opMUL(armregs[MULRN]);
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
                                        templ2 = readmeml(addr);
                                        cache_read_timing(addr, 1, 0);
                                        if (!databort)
                                        {
                                                writememl(addr, templ);
                                                cache_write_timing(addr, 1);
                                                if (!databort)
                                                {
                                                        LOADREG(RD, templ2);
                                                }
                                        }
                                        promote_fetch_to_n = PROMOTE_NOMERGE;
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
                                        templ2 = readmemb(addr);
                                        cache_read_timing(addr, 1, 0);
                                        if (!databort)
                                        {
                                                writememb(addr, templ);
                                                cache_write_timing(addr, 1);
                                                if (!databort)
                                                {
                                                        LOADREG(RD, templ2);
                                                }
                                        }
                                        promote_fetch_to_n = PROMOTE_NOMERGE;
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
                                cache_read_timing(addr, 1, 0);
                                LOADREG(RD, templ2);
                                if (!(opcode & 0x1000000))
                                {
                                        addr += addr2;
                                        armregs[RN] = addr;
                                }
                                else if (opcode&0x200000)
                                        armregs[RN]=addr;
                                merge_timing(PC+4);
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
                                cache_read_timing(addr, 1, 0);
                                LOADREG(RD, templ2);
                                if (opcode & 0x800000)
                                        armregs[RN] += addr2;
                                else
                                        armregs[RN] -= addr2;
                                merge_timing(PC+4);
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
                                cache_read_timing(addr, 1, 0);
                                LOADREG(RD, templ2);
                                if (opcode & 0x800000)
                                        armregs[RN] += addr2;
                                else
                                        armregs[RN] -= addr2;
                                merge_timing(PC+4);
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
                                promote_fetch_to_n = PROMOTE_NOMERGE;
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
                                promote_fetch_to_n = PROMOTE_NOMERGE;
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
                                promote_fetch_to_n = PROMOTE_NOMERGE;
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
                                promote_fetch_to_n = PROMOTE_NOMERGE;
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
                                promote_fetch_to_n = PROMOTE_NOMERGE;
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
                                promote_fetch_to_n = PROMOTE_NOMERGE;
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
                                cache_read_timing(addr, 1, 0);
                                if (!(opcode & 0x1000000))
                                {
                                        addr += addr2;
                                        armregs[RN] = addr;
                                }
                                else if (opcode & 0x200000)
                                        armregs[RN] = addr;
                                LOADREG(RD, templ);
                                merge_timing(PC+4);
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
                                cache_read_timing(addr, 1, 0);
                                if (!(opcode & 0x1000000))
                                {
                                        addr += addr2;
                                        armregs[RN] = addr;
                                }
                                else if (opcode & 0x200000)
                                        armregs[RN] = addr;
                                armregs[RD] = templ;
                                merge_timing(PC+4);
                                break;

#define STMfirst()      mask=1; \
                        CHECK_ADDR_EXCEPTION(addr); \
                        for (c = 0; c < 16; c++) \
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
                        
#define STMall()        for (; c < 16; c++) \
                        { \
                                if (opcode & mask) \
                                { \
                                        if (c == 15) { writememl(addr, armregs[c] + 4); } \
                                        else         { writememl(addr, armregs[c]); } \
                                        cache_write_timing(addr, !(addr & 0xc)); \
                                        addr += 4; \
                                } \
                                mask <<= 1; \
                        }

#define STMfirstS()     mask = 1; \
                        CHECK_ADDR_EXCEPTION(addr); \
                        for (c = 0; c < 16; c++) \
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

#define STMallS()       for (; c < 16; c++) \
                        { \
                                if (opcode & mask) \
                                { \
                                        if (c == 15) { writememl(addr, armregs[c] + 4); } \
                                        else         { writememl(addr, *usrregs[c]); } \
                                        cache_write_timing(addr, !(addr & 0xc)); \
                                        addr += 4; \
                                } \
                                mask <<= 1; \
                        }

#define LDMall()        mask = 1; \
                        CHECK_ADDR_EXCEPTION(addr); \
                        first_access = 1; \
                        for (c = 0; c < 15; c++) \
                        { \
                                if (opcode & mask) \
                                { \
                                        templ = readmeml(addr); if (!databort) armregs[c] = templ; \
                                        cache_read_timing(addr, first_access || !(addr & 0xc), 0); \
                                        first_access = 0; \
                                        addr+=4; \
                                } \
                                mask<<=1; \
                        } \
                        if (opcode & 0x8000) \
                        { \
                                templ = readmeml(addr); \
                        	cache_read_timing(addr, first_access || !(addr & 0xc), 0); \
                                addr += 4; \
                                if (!databort) armregs[15] = (armregs[15] & 0xFC000003) | ((templ+4) & 0x3FFFFFC); \
                                refillpipeline(); \
                        }

#define LDMallS()       mask = 1; \
                        CHECK_ADDR_EXCEPTION(addr); \
                        first_access = 1; \
                        if (opcode & 0x8000) \
                        { \
                                for (c = 0; c < 15; c++) \
                                { \
                                        if (opcode & mask) \
                                        { \
                                                templ = readmeml(addr); if (!databort) armregs[c] = templ; \
		                        	cache_read_timing(addr, first_access || !(addr & 0xc), 0); \
                                                first_access = 0; \
                                                addr += 4; \
                                        } \
                                        mask <<= 1; \
                                } \
                                templ = readmeml(addr); \
                        	cache_read_timing(addr, first_access || !(addr & 0xc), 0); \
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
		                        	cache_read_timing(addr, first_access || !(addr & 0xc), 0); \
                                                first_access = 0; \
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
                                promote_fetch_to_n = PROMOTE_NOMERGE;
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
                                promote_fetch_to_n = PROMOTE_NOMERGE;
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
                                promote_fetch_to_n = PROMOTE_NOMERGE;
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
                                promote_fetch_to_n = PROMOTE_NOMERGE;
                                break;
                                
                                case 0x81: /*LDMDA*/
                                case 0x83: /*LDMDA !*/
                                case 0x91: /*LDMDB*/
                                case 0x93: /*LDMDB !*/
                                addr = armregs[RN] - countbits(opcode & 0xFFFF);
                                if (!(opcode & 0x1000000))
                                        addr += 4;
                                if ((opcode & 0x200000) && (RN != 15))
                                        armregs[RN] -= countbits(opcode & 0xFFFF);
                                LDMall();
                                merge_timing(PC+4);
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
                                merge_timing(PC+4);
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
                                merge_timing(PC+4);
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
                                merge_timing(PC+4);
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
                                        else
                                                promote_fetch_to_n = PROMOTE_NOMERGE;
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

		if (TIMER_VAL_LESS_THAN_VAL(timer_target, tsc >> 32))
			timer_process();

                cyc = (int)((tsc - oldcyc) >> 32); /*Number of clock ticks executed*/
                clock_ticks_executed += cyc;
                total_cycles -= (tsc - oldcyc);

        }
        LOG_EVENT_LOOP("execarm() finished; clock ticks=%d, and called pollline() %d times (should be ~160)\n",
                clock_ticks_executed, pollline_call_count);
}
