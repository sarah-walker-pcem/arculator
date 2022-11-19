/*Arculator 2.1 by Sarah Walker
  ARM2 & ARM3 emulation*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arc.h"
#include "arm.h"
#include "cp15.h"
#include "debugger.h"
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
static uint64_t min_timer;
static int next_dma_source;
void recalc_min_timer(void)
{
	min_timer = refresh_ts;
	next_dma_source = DMA_REFRESH;

	if (memc_dma_sound_req && TIMER_VAL_LESS_THAN_VAL_64(memc_dma_sound_req_ts, min_timer))
	{
		min_timer = memc_dma_sound_req_ts;
		next_dma_source = DMA_SOUND;
	}
	if (memc_videodma_enable && memc_dma_cursor_req && TIMER_VAL_LESS_THAN_VAL_64(memc_dma_cursor_req_ts, min_timer))
	{
		min_timer = memc_dma_cursor_req_ts;
		next_dma_source = DMA_CURSOR;
	}
	if (memc_videodma_enable && memc_dma_video_req && TIMER_VAL_LESS_THAN_VAL_64(memc_dma_video_req_ts, min_timer))
	{
		min_timer = memc_dma_video_req_ts;
		next_dma_source = DMA_VIDEO;
	}
}

static void run_dma(int update_tsc)
{
	while (TIMER_VAL_LESS_THAN_VAL_64(min_timer, tsc))
	{
		switch (next_dma_source)
		{
			case DMA_REFRESH:
//                        if (output) rpclog("Refresh DMA %i\n", mem_dorefresh);
			if (mem_dorefresh)
			{
				if (TIMER_VAL_LESS_THAN_VAL_64(refresh_ts, mem_available_ts))
					mem_available_ts += mem_spd_multi_2;
				else
					mem_available_ts = refresh_ts + mem_spd_multi_2;
			}
			refresh_ts += mem_spd_multi_32;
			break;
			case DMA_SOUND:
//                        if (output) rpclog("Sound DMA\n");
			if (TIMER_VAL_LESS_THAN_VAL_64(memc_dma_sound_req_ts, mem_available_ts))
				mem_available_ts += mem_spd_multi_5;
			else
				mem_available_ts = memc_dma_sound_req_ts + mem_spd_multi_5;
			memc_dma_sound_req = 0;
			break;
			case DMA_CURSOR:
//                        if (output) rpclog("Cursor DMA\n");
			if (TIMER_VAL_LESS_THAN_VAL_64(memc_dma_cursor_req_ts, mem_available_ts))
				mem_available_ts += mem_spd_multi_5;
			else
				mem_available_ts = memc_dma_cursor_req_ts + mem_spd_multi_5;
			memc_dma_cursor_req = 0;
			break;
			case DMA_VIDEO:
//                        if (output) rpclog("Video fetch %i\n", memc_dma_video_req);
			if (TIMER_VAL_LESS_THAN_VAL_64(memc_dma_video_req_ts, mem_available_ts))
				mem_available_ts += memc_dma_video_req * mem_spd_multi_5;
			else
				mem_available_ts = memc_dma_video_req_ts + memc_dma_video_req * mem_spd_multi_5;
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
		recalc_min_timer();
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
		tsc = (tsc + 0xffffffffull) & ~0xffffffffull;

		clock_domain = DOMAIN_FCLK;
	}
}

static uint64_t last_cycle_length = 0;

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

static void cache_line_fill(uint32_t addr)
{
	int byte_offset = addr >> (4+3);
	int bit_offset = (addr >> 4) & 7;
	int set = (addr >> 4) & 3;

#ifndef RELEASE_BUILD
	if (addr & ~0x3ffffff)
		fatal("cache_line_fill outside of valid range %08x\n", addr);
#endif

	if (arm3_cache_tag[set][arm3_slot] != TAG_INVALID)
	{
		int old_bit_offset = (arm3_cache_tag[set][arm3_slot] >> 4) & 7;
		int old_byte_offset = (arm3_cache_tag[set][arm3_slot] >> (4+3));

		arm3_cache[old_byte_offset] &= ~(1 << old_bit_offset);
	}

	arm3_cache_tag[set][arm3_slot] = addr & ~0xf;
	arm3_cache[byte_offset] |= (1 << bit_offset);
	sync_to_mclk();

	mem_available_ts = tsc + mem_speed[addr >> 12][1] + 3*mem_speed[addr >> 12][0];

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
#ifndef RELEASE_BUILD
	if (addr & ~0x3ffffff)
		fatal("cache_read_timing outside of valid range %08x\n", addr);
#endif

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

			int bit_offset = (addr >> 4) & 7;
			int byte_offset = addr >> (4+3);

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
				cache_line_fill(addr);
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
			else if (cache_was_on)
			{
#ifndef RELEASE_BUILD
				int bit_offset = (addr >> 4) & 7;
				int byte_offset = ((addr & 0x3ffffff) >> (4+3));

				if (!(arm3_cache[byte_offset] & (1 << bit_offset)))
					fatal("S-cycle - cache_was_on but data not in cache %08x\n", addr);
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
				cache_line_fill(addr);
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

	cache_was_on = 0;

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
uint32_t rotatelookup[4096];
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
		memmode = osmode ? MEMMODE_OS : MEMMODE_USER;
		for (c=0;c<15;c++) usrregs[c]=&armregs[c];
		break;
		case IRQ:
		for (c=8;c<13;c++) armregs[c]=userregs[c];
		armregs[13]=irqregs[0];
		armregs[14]=irqregs[1];
		for (c=0;c<13;c++) usrregs[c]=&armregs[c];
		for (c=13;c<15;c++) usrregs[c]=&userregs[c];
		memmode = MEMMODE_SUPER;
		break;
		case FIQ:
		for (c=8;c<15;c++) armregs[c]=fiqregs[c];
		for (c=0;c<8;c++)  usrregs[c]=&armregs[c];
		for (c=8;c<15;c++) usrregs[c]=&userregs[c];
		memmode = MEMMODE_SUPER;
		break;
		case SUPERVISOR:
		for (c=8;c<13;c++) armregs[c]=userregs[c];
		armregs[13]=superregs[0];
		armregs[14]=superregs[1];
		for (c=0;c<13;c++) usrregs[c]=&armregs[c];
		for (c=13;c<15;c++) usrregs[c]=&userregs[c];
		memmode = MEMMODE_SUPER;
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
	memmode = MEMMODE_SUPER;
	memstat[0] = 1;
	mempoint[0] = (uint8_t *)rom;
	refillpipeline2();
	resetcp15();
	resetfpa();

	memset(arm3_cache, 0, sizeof(arm3_cache));
	memset(arm3_cache_tag, TAG_INVALID, sizeof(arm3_cache_tag));

	ins = 0;
	tsc = 0;
	mem_available_ts = 0;
	refresh_ts = 0;
	clock_domain = DOMAIN_MCLK;
	pending_reads = 0;
	promote_fetch_to_n = PROMOTE_NONE;
	prefabort = 0;
	databort = 0;
}

int indumpregs=0;

void dumpregs()
{
	int c;
	FILE *f;
	uint32_t l;

	if (indumpregs) return;
	indumpregs=1;

	memmode = MEMMODE_SUPER;

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
	const int shiftmode = (opcode >> 5) & 3;
	int shiftamount = (opcode >> 7) & 31;
	const uint32_t temp = armregs[RM];

	if (opcode & 0x10)
	{
		shiftamount = armregs[(opcode >> 8) & 15] & 0xFF;
		merge_timing(PC+4);
	}

	switch (shiftmode)
	{
		case 0: /*LSL*/
		if (!shiftamount)
			return temp;
		if (shiftamount>=32)
			return 0;
		return temp << shiftamount;

		case 1: /*LSR*/
		if ((!shiftamount && !(opcode & 0x10)) || (shiftamount >= 32))
			return 0;
		return temp >> shiftamount;

		case 2: /*ASR*/
		if ((!shiftamount && !(opcode & 0x10)) || (shiftamount >= 32))
			return (int32_t)temp >> 31;
		return (int32_t)temp >> shiftamount;

		case 3: /*ROR*/
		shiftamount &= 0x1f;
		if (!shiftamount && !(opcode & 0x10))
			return (((CFSET) ? 1 : 0) << 31) | (temp >> 1);
		if (!shiftamount)
			return temp;
		return (temp >> shiftamount) | (temp << (32 - shiftamount));
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
	const int shiftmode = (opcode >> 5) & 3;
	const int shiftamount = (opcode >> 7) & 31;
	const uint32_t temp = armregs[RM];

#ifndef RELEASE_BUILD
	if (opcode&0x10)
		fatal("Shift by register on memory shift!!! %08X\n",PC);
#endif

	switch (shiftmode)
	{
		case 0: /*LSL*/
		if (!shiftamount)
			return temp;
		return temp << shiftamount;

		case 1: /*LSR*/
		if (!shiftamount)
			return 0;
		return temp >> shiftamount;

		case 2: /*ASR*/
		if (!shiftamount)
			return (int32_t)temp >> 31;
		return (int32_t)temp >> shiftamount;

		case 3: /*ROR*/
		if (!shiftamount)
			return (((CFSET) ? 1 : 0) << 31) | (temp >> 1);
		return (temp >> shiftamount) | (temp << (32 - shiftamount));
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
					pccache2 = (uint32_t *)mempoint[templ2]; \
					opcode = pccache2[(addr & 0xFFF) >> 2]; \
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
	uint32_t templ,templ2,addr = (PC-4) & 0x3fffffc;

	prefabort_next = 0;
//        if ((armregs[15]&0x3FFFFFC)==8) rpclog("illegal instruction %08X at %07X\n",opcode,opc);
	readmemfff(addr,opcode2);
	addr = (addr + 4) & 0x3fffffc;
	prefabort = prefabort_next;
	readmemfff(addr,opcode3);

	cache_read_timing((PC-4) & 0x3fffffc, 1, 0);
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

	cache_read_timing((PC-8) & 0x3fffffc, 1, 0);
	cache_read_timing((PC-4) & 0x3fffffc, !((PC-4) & 0xc), 0);
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

#define EXCEPTION_UNDEFINED()                                 \
	do                                                    \
	{                                                     \
		if (debugon)                                  \
			debug_trap(DEBUG_TRAP_UNDEF, opcode); \
		exception(0x08, SUPERVISOR, -4); \
	} while (0)

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

#define CHECK_ADDR_EXCEPTION(a) if ((a) & 0xfc000000) { databort = 2; return; }

static int64_t total_cycles;

typedef void (*OpFn)(uint32_t opcode);

static void opNULL(uint32_t opcode)
{
}

static void opUNDEF(uint32_t opcode)
{
	rpclog("Illegal instruction %08X %07X\n", opcode, PC);
	EXCEPTION_UNDEFINED();
}

static void opANDreg(uint32_t opcode)
{
	if ((opcode&0x90) == 0x90) /*MUL*/
	{
		opMUL(0);
	}
	else
	{
		uint32_t templ = shift_noflags(opcode);

		if (RD == 15)
			LOAD_R15(GETADDR(RN) & templ);
		else
			armregs[RD] = GETADDR(RN) & templ;
	}
}

static void opANDregS(uint32_t opcode)
{
	if ((opcode & 0x90) == 0x90) /*MULS*/
	{
		opMUL(0);
		setzn(armregs[MULRD]);
	}
	else
	{
		if (RD == 15)
		{
			uint32_t templ = shift_noflags(opcode);
			LOAD_R15_S(GETADDR(RN) & templ);
		}
		else
		{
			uint32_t templ = shift(opcode);
			armregs[RD] = GETADDR(RN) & templ;
			setzn(armregs[RD]);
		}
	}
}

static void opEORreg(uint32_t opcode)
{
	if ((opcode & 0x90) == 0x90) /*MLA*/
	{
		opMUL(armregs[MULRN]);
	}
	else
	{
		uint32_t templ = shift_noflags(opcode);

		if (RD == 15)
			LOAD_R15(GETADDR(RN) ^ templ);
		else
			armregs[RD] = GETADDR(RN) ^ templ;
	}
}

static void opEORregS(uint32_t opcode)
{
	if ((opcode & 0x90) == 0x90) /*MLAS*/
	{
		opMUL(armregs[MULRN]);
		setzn(armregs[MULRD]);
	}
	else
	{
		if (RD == 15)
		{
			uint32_t templ = shift_noflags(opcode);
			LOAD_R15_S(GETADDR(RN) ^ templ);
		}
		else
		{
			uint32_t templ = shift(opcode);
			armregs[RD] = GETADDR(RN) ^ templ;
			setzn(armregs[RD]);
		}
	}
}

static void opSUBreg(uint32_t opcode)
{
	uint32_t templ = shift_noflags(opcode);

	if (RD == 15)
		LOAD_R15(GETADDR(RN) - templ);
	else
		armregs[RD] = GETADDR(RN) - templ;
}

static void opSUBregS(uint32_t opcode)
{
	uint32_t templ = shift_noflags(opcode);

	if (RD == 15)
		LOAD_R15_S(GETADDR(RN) - templ);
	else
	{
		uint32_t templ = shift_noflags(opcode);
		setsub(GETADDR(RN), templ, GETADDR(RN) - templ);
		armregs[RD] = GETADDR(RN) - templ;
	}
}

static void opRSBreg(uint32_t opcode)
{
	uint32_t templ = shift_noflags(opcode);

	if (RD == 15)
		LOAD_R15(templ - GETADDR(RN));
	else
		armregs[RD] = templ - GETADDR(RN);
}

static void opRSBregS(uint32_t opcode)
{
	uint32_t templ = shift_noflags(opcode);

	if (RD == 15)
		LOAD_R15_S(templ - GETADDR(RN));
	else
	{
		setsub(templ, GETADDR(RN), templ - GETADDR(RN));
		armregs[RD] = templ - GETADDR(RN);
	}
}

static void opADDreg(uint32_t opcode)
{
	uint32_t templ = shift_noflags(opcode);

	if (RD == 15)
		LOAD_R15(GETADDR(RN) + templ);
	else
		armregs[RD] = GETADDR(RN) + templ;
}

static void opADDregS(uint32_t opcode)
{
	uint32_t templ = shift_noflags(opcode);

	if (RD == 15)
		LOAD_R15_S(GETADDR(RN) + templ);
	else
	{
		setadd(GETADDR(RN), templ, GETADDR(RN) + templ);
		armregs[RD] = GETADDR(RN) + templ;
	}
}

static void opADCreg(uint32_t opcode)
{
	uint32_t templ2 = CFSET;
	uint32_t templ = shift_noflags(opcode);

	if (RD == 15)
		LOAD_R15(GETADDR(RN) + templ + templ2);
	else
		armregs[RD] = GETADDR(RN) + templ + templ2;
}

static void opADCregS(uint32_t opcode)
{
	uint32_t templ2 = CFSET;
	uint32_t templ = shift_noflags(opcode);

	if (RD == 15)
		LOAD_R15_S(GETADDR(RN) + templ + templ2);
	else
	{
		setadc(GETADDR(RN), templ, GETADDR(RN) + templ + templ2);
		armregs[RD] = GETADDR(RN) + templ + templ2;
	}
}

static void opSBCreg(uint32_t opcode)
{
	uint32_t templ2 = CFSET ? 0 : 1;
	uint32_t templ = shift_noflags(opcode);

	if (RD == 15)
		LOAD_R15(GETADDR(RN) - (templ + templ2));
	else
		armregs[RD] = GETADDR(RN) - (templ + templ2);
}

static void opSBCregS(uint32_t opcode)
{
	uint32_t templ2 = CFSET ? 0 : 1;
	uint32_t templ = shift_noflags(opcode);

	if (RD == 15)
		LOAD_R15_S(GETADDR(RN) - (templ + templ2));
	else
	{
		setsbc(GETADDR(RN), templ, GETADDR(RN) - (templ + templ2));
		armregs[RD] = GETADDR(RN) - (templ + templ2);
	}
}

static void opRSCreg(uint32_t opcode)
{
	uint32_t templ2 = CFSET ? 0 : 1;
	uint32_t templ = shift_noflags(opcode);

	if (RD == 15)
		LOAD_R15(templ - (GETADDR(RN) + templ2));
	else
		armregs[RD] = templ - (GETADDR(RN) + templ2);
}

static void opRSCregS(uint32_t opcode)
{
	uint32_t templ2 = CFSET ? 0 : 1;
	uint32_t templ = shift_noflags(opcode);

	if (RD == 15)
		LOAD_R15_S(templ - (GETADDR(RN) + templ2));
	else
	{
		setsbc(templ, GETADDR(RN), templ - (GETADDR(RN) + templ2));
		armregs[RD] = templ - (GETADDR(RN) + templ2);
	}
}

static void opSWP(uint32_t opcode)
{
	if ((opcode & 0xf0) != 0x90)
		return;
	if (arm_has_swp)
	{
		uint32_t addr = GETADDR(RN);
		CHECK_ADDR_EXCEPTION(addr);
		uint32_t templ = GETREG(RM);
		uint32_t templ2 = readmeml(addr);
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
}

static void opTSTreg(uint32_t opcode)
{
	if (RD == 15)
	{
		uint32_t src_data = shift_noflags(opcode);

		if (armregs[15] & 3)
		{
			uint32_t templ = armregs[15] & 0x3FFFFFC;
			armregs[15] = ((GETADDR(RN) & src_data) & 0xFC000003) | templ;
			if ((armregs[15] & 3) != mode)
				updatemode(armregs[15] & 3);
		}
		else
		{
			uint32_t templ = armregs[15] & 0x0FFFFFFF;
			armregs[15] = ((GETADDR(RN) & src_data) & 0xF0000000) | templ;
		}
	}
	else
		setzn(GETADDR(RN) & shift(opcode));
}

static void opTEQreg(uint32_t opcode)
{
	if (RD == 15)
	{
		uint32_t src_data = shift_noflags(opcode);

		if (armregs[15] & 3)
		{
			uint32_t templ = armregs[15] & 0x3FFFFFC;
			armregs[15] = ((GETADDR(RN) ^ src_data) & 0xFC000003) | templ;
			if ((armregs[15] & 3) != mode)
				updatemode(armregs[15] & 3);
		}
		else
		{
			uint32_t templ = armregs[15] & 0x0FFFFFFF;
			armregs[15] = ((GETADDR(RN) ^ src_data) & 0xF0000000) | templ;
		}
	}
	else
		setzn(GETADDR(RN) ^ shift(opcode));
}

static void opSWPB(uint32_t opcode)
{
	if ((opcode & 0xf0) != 0x90)
		return;
	if (arm_has_swp)
	{
		uint32_t addr = armregs[RN];
		CHECK_ADDR_EXCEPTION(addr);
		uint32_t templ = GETREG(RM);
		uint32_t templ2 = readmemb(addr);
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
}

static void opCMPreg(uint32_t opcode)
{
	uint32_t src_data = shift_noflags(opcode);

	if (RD == 15)
	{
		if (armregs[15] & 3)
		{
			uint32_t templ = armregs[15] & 0x3FFFFFC;
			armregs[15] = ((GETADDR(RN) - src_data) & 0xFC000003) | templ;
			if ((armregs[15] & 3) != mode)
				updatemode(armregs[15] & 3);
		}
		else
		{
			uint32_t templ = armregs[15] & 0x0FFFFFFF;
			armregs[15] = ((GETADDR(RN) - src_data) & 0xF0000000) | templ;
		}
	}
	else
		setsub(GETADDR(RN), src_data, GETADDR(RN) - src_data);
}

static void opCMNreg(uint32_t opcode)
{
	uint32_t src_data = shift_noflags(opcode);

	if (RD == 15)
	{
		if (armregs[15] & 3)
		{
			uint32_t templ = armregs[15] & 0x3FFFFFC;
			armregs[15] = ((GETADDR(RN) + src_data) & 0xFC000003) | templ;
			if ((armregs[15] & 3) != mode)
				updatemode(armregs[15] & 3);
		}
		else
		{
			uint32_t templ = armregs[15] & 0x0FFFFFFF;
			armregs[15] = ((GETADDR(RN) + src_data) & 0xF0000000) | templ;
		}
	}
	else
		setadd(GETADDR(RN), src_data, GETADDR(RN) + src_data);
}

static void opORRreg(uint32_t opcode)
{
	uint32_t templ = shift_noflags(opcode);

	if (RD == 15)
		LOAD_R15(GETADDR(RN) | templ);
	else
		armregs[RD] = GETADDR(RN) | templ;
}

static void opORRregS(uint32_t opcode)
{
	if (RD == 15)
	{
		uint32_t templ = shift_noflags(opcode);
		LOAD_R15_S(GETADDR(RN) | templ);
	}
	else
	{
		uint32_t templ = shift(opcode);
		armregs[RD] = GETADDR(RN) | templ;
		setzn(armregs[RD]);
	}
}

static void opMOVreg(uint32_t opcode)
{
	if (RD == 15)
		LOAD_R15(shift_noflags(opcode));
	else
		armregs[RD] = shift_noflags(opcode);
}

static void opMOVregS(uint32_t opcode)
{
	if (RD == 15)
		LOAD_R15_S(shift_noflags(opcode));
	else
	{
		armregs[RD] = shift(opcode);
		setzn(armregs[RD]);
	}
}

static void opBICreg(uint32_t opcode)
{
	uint32_t templ = shift_noflags(opcode);

	if (RD == 15)
		LOAD_R15(GETADDR(RN) & ~templ);
	else
		armregs[RD] = GETADDR(RN) & ~templ;
}

static void opBICregS(uint32_t opcode)
{
	if (RD == 15)
	{
		uint32_t templ = shift_noflags(opcode);
		LOAD_R15_S(GETADDR(RN) & ~templ);
	}
	else
	{
		uint32_t templ = shift(opcode);
		armregs[RD] = GETADDR(RN) & ~templ;
		setzn(armregs[RD]);
	}
}

static void opMVNreg(uint32_t opcode)
{
	if (RD == 15)
		LOAD_R15(~shift_noflags(opcode));
	else
		armregs[RD] = ~shift_noflags(opcode);
}

static void opMVNregS(uint32_t opcode)
{
	if (RD == 15)
		LOAD_R15_S(~shift_noflags(opcode));
	else
	{
		armregs[RD] = ~shift(opcode);
		setzn(armregs[RD]);
	}
}

static void opANDimm(uint32_t opcode)
{
	uint32_t templ = rotate_noflags(opcode);

	if (RD == 15)
		LOAD_R15(GETADDR(RN) & templ);
	else
		armregs[RD] = GETADDR(RN) & templ;
}

static void opANDimmS(uint32_t opcode)
{
	if (RD == 15)
	{
		uint32_t templ = rotate_noflags(opcode);
		LOAD_R15_S(GETADDR(RN) & templ);
	}
	else
	{
		uint32_t templ = rotate(opcode);
		armregs[RD] = GETADDR(RN) & templ;
		setzn(armregs[RD]);
	}
}

static void opEORimm(uint32_t opcode)
{
	uint32_t templ = rotate_noflags(opcode);

	if (RD == 15)
		LOAD_R15(GETADDR(RN) ^ templ);
	else
		armregs[RD] = GETADDR(RN) ^ templ;
}

static void opEORimmS(uint32_t opcode)
{
	if (RD == 15)
	{
		uint32_t templ = rotate_noflags(opcode);
		LOAD_R15_S(GETADDR(RN) ^ templ);
	}
	else
	{
		uint32_t templ = rotate(opcode);
		armregs[RD] = GETADDR(RN) ^ templ;
		setzn(armregs[RD]);
	}
}

static void opSUBimm(uint32_t opcode)
{
	uint32_t templ = rotate_noflags(opcode);

	if (RD == 15)
		LOAD_R15(GETADDR(RN) - templ);
	else
		armregs[RD] = GETADDR(RN) - templ;
}

static void opSUBimmS(uint32_t opcode)
{
	uint32_t templ = rotate_noflags(opcode);

	if (RD == 15)
		LOAD_R15_S(GETADDR(RN) - templ);
	else
	{
		setsub(GETADDR(RN), templ, GETADDR(RN) - templ);
		armregs[RD] = GETADDR(RN) - templ;
	}
}

static void opRSBimm(uint32_t opcode)
{
	uint32_t templ = rotate_noflags(opcode);

	if (RD == 15)
		LOAD_R15(templ - GETADDR(RN));
	else
		armregs[RD] = templ - GETADDR(RN);
}

static void opRSBimmS(uint32_t opcode)
{
	uint32_t templ = rotate_noflags(opcode);

	if (RD == 15)
		LOAD_R15_S(templ - GETADDR(RN));
	else
	{
		setsub(templ, GETADDR(RN), templ - GETADDR(RN));
		armregs[RD] = templ - GETADDR(RN);
	}
}

static void opADDimm(uint32_t opcode)
{
	uint32_t templ = rotate_noflags(opcode);

	if (RD == 15)
		LOAD_R15(GETADDR(RN) + templ);
	else
		armregs[RD] = GETADDR(RN) + templ;
}

static void opADDimmS(uint32_t opcode)
{
	uint32_t templ = rotate_noflags(opcode);

	if (RD == 15)
		LOAD_R15_S(GETADDR(RN) + templ);
	else
	{
		setadd(GETADDR(RN), templ, GETADDR(RN) + templ);
		armregs[RD] = GETADDR(RN) + templ;
	}
}

static void opADCimm(uint32_t opcode)
{
	uint32_t templ2 = CFSET;
	uint32_t templ = rotate_noflags(opcode);

	if (RD == 15)
		LOAD_R15(GETADDR(RN) + templ + templ2);
	else
		armregs[RD] = GETADDR(RN) + templ + templ2;
}

static void opADCimmS(uint32_t opcode)
{
	uint32_t templ2 = CFSET;
	uint32_t templ = rotate_noflags(opcode);

	if (RD == 15)
		LOAD_R15_S(GETADDR(RN) + templ + templ2);
	else
	{
		setadc(GETADDR(RN), templ,GETADDR(RN) + templ + templ2);
		armregs[RD] = GETADDR(RN) + templ + templ2;
	}
}

static void opSBCimm(uint32_t opcode)
{
	uint32_t templ2 = CFSET ? 0 : 1;
	uint32_t templ = rotate_noflags(opcode);

	if (RD == 15)
		LOAD_R15(GETADDR(RN) - (templ + templ2));
	else
		armregs[RD] = GETADDR(RN) - (templ + templ2);
}

static void opSBCimmS(uint32_t opcode)
{
	uint32_t templ2 = CFSET ? 0 : 1;
	uint32_t templ = rotate_noflags(opcode);

	if (RD == 15)
		LOAD_R15_S(GETADDR(RN) - (templ + templ2));
	else
	{
		setsbc(GETADDR(RN), templ, GETADDR(RN) - (templ + templ2));
		armregs[RD] = GETADDR(RN) - (templ + templ2);
	}
}

static void opRSCimm(uint32_t opcode)
{
	uint32_t templ2 = CFSET ? 0 : 1;
	uint32_t templ = rotate_noflags(opcode);

	if (RD == 15)
		LOAD_R15(templ - (GETADDR(RN) + templ2));
	else
		armregs[RD] = templ - (GETADDR(RN) + templ2);
}

static void opRSCimmS(uint32_t opcode)
{
	uint32_t templ2 = CFSET ? 0 : 1;
	uint32_t templ = rotate_noflags(opcode);

	if (RD == 15)
		LOAD_R15_S(templ - (GETADDR(RN) + templ2));
	else
	{
		setsbc(templ, GETADDR(RN), templ - (GETADDR(RN) + templ2));
		armregs[RD] = templ - (GETADDR(RN) + templ2);
	}
}

static void opTSTimm(uint32_t opcode)
{
	if (RD == 15)
	{
		uint32_t src_data = rotate_noflags(opcode);

		if (armregs[15] & 3)
		{
			uint32_t templ = armregs[15] & 0x3FFFFFC;
			armregs[15] = ((GETADDR(RN) & src_data) & 0xFC000003) | templ;
			if ((armregs[15] & 3) != mode)
				updatemode(armregs[15] & 3);
		}
		else
		{
			uint32_t templ = armregs[15] & 0x0FFFFFFF;
			armregs[15] = ((GETADDR(RN) & src_data) & 0xF0000000) | templ;
		}
	}
	else
		setzn(GETADDR(RN) & rotate(opcode));
}

static void opTEQimm(uint32_t opcode)
{
	if (RD == 15)
	{
		uint32_t src_data = rotate_noflags(opcode);

		if (armregs[15] & 3)
		{
			uint32_t templ = armregs[15] & 0x3FFFFFC;
			armregs[15] = ((GETADDR(RN) ^ src_data) & 0xFC000003) | templ;
			if ((armregs[15] & 3) != mode)
				updatemode(armregs[15] & 3);
		}
		else
		{
			uint32_t templ = armregs[15] & 0x0FFFFFFF;
			armregs[15] = ((GETADDR(RN) ^ src_data) & 0xF0000000) | templ;
		}
	}
	else
		setzn(GETADDR(RN) ^ rotate(opcode));
}

static void opCMPimm(uint32_t opcode)
{
	uint32_t src_data = rotate_noflags(opcode);

	if (RD == 15)
	{
		if (armregs[15] & 3)
		{
			uint32_t templ = armregs[15] & 0x3FFFFFC;
			armregs[15] = ((GETADDR(RN) - src_data) & 0xFC000003) | templ;
			if ((armregs[15] & 3) != mode)
				updatemode(armregs[15] & 3);
		}
		else
		{
			uint32_t templ = armregs[15] & 0x0FFFFFFF;
			armregs[15] = ((GETADDR(RN) - src_data) & 0xF0000000) | templ;
		}
	}
	else
		setsub(GETADDR(RN), src_data, GETADDR(RN) - src_data);
}

static void opCMNimm(uint32_t opcode)
{
	uint32_t src_data = rotate_noflags(opcode);

	if (RD == 15)
	{
		if (armregs[15] & 3)
		{
			uint32_t templ = armregs[15] & 0x3FFFFFC;
			armregs[15] = ((GETADDR(RN) + src_data) & 0xFC000003) | templ;
			if ((armregs[15] & 3) != mode)
				updatemode(armregs[15] & 3);
		}
		else
		{
			uint32_t templ = armregs[15] & 0x0FFFFFFF;
			armregs[15] = ((GETADDR(RN) + src_data) & 0xF0000000) | templ;
		}
	}
	else
		setadd(GETADDR(RN), src_data, GETADDR(RN) + src_data);
}

static void opORRimm(uint32_t opcode)
{
	uint32_t templ = rotate_noflags(opcode);

	if (RD == 15)
		LOAD_R15(GETADDR(RN) | templ);
	else
		armregs[RD] = GETADDR(RN) | templ;
}

static void opORRimmS(uint32_t opcode)
{
	if (RD == 15)
	{
		uint32_t templ = rotate_noflags(opcode);
		LOAD_R15_S(GETADDR(RN) | templ);
	}
	else
	{
		uint32_t templ = rotate(opcode);
		armregs[RD] = GETADDR(RN) | templ;
		setzn(armregs[RD]);
	}
}

static void opMOVimm(uint32_t opcode)
{
	if (RD == 15)
		LOAD_R15(rotate_noflags(opcode));
	else
		armregs[RD] = rotate_noflags(opcode);
}

static void opMOVimmS(uint32_t opcode)
{
	if (RD == 15)
		LOAD_R15_S(rotate_noflags(opcode));
	else
	{
		armregs[RD] = rotate(opcode);
		setzn(armregs[RD]);
	}
}

static void opBICimm(uint32_t opcode)
{
	uint32_t templ = rotate_noflags(opcode);

	if (RD == 15)
		LOAD_R15(GETADDR(RN) & ~templ);
	else
		armregs[RD] = GETADDR(RN) & ~templ;
}

static void opBICimmS(uint32_t opcode)
{
	if (RD == 15)
	{
		uint32_t templ = rotate_noflags(opcode);
		LOAD_R15_S(GETADDR(RN) & ~templ);
	}
	else
	{
		uint32_t templ = rotate(opcode);
		armregs[RD] = GETADDR(RN) & ~templ;
		setzn(armregs[RD]);
	}
}

static void opMVNimm(uint32_t opcode)
{
	if (RD == 15)
		LOAD_R15(~rotate_noflags(opcode));
	else
		armregs[RD] = ~rotate_noflags(opcode);
}

static void opMVNimmS(uint32_t opcode)
{
	if (RD == 15)
		LOAD_R15_S(~rotate_noflags(opcode));
	else
	{
		armregs[RD] = ~rotate(opcode);
		setzn(armregs[RD]);
	}
}

#define opLDR(_op)								\
	static void opLDR##_op(uint32_t opcode)					\
	{									\
		uint32_t addr, offset;						\
		uint32_t templ;							\
		int old_memmode = memmode;					\
		const uint8_t op = 0x##_op;					\
										\
		addr = GETADDR(RN);						\
		if (op & 0x20)							\
		{								\
			if (opcode & 0x10) /*Shift by register*/		\
			{							\
				EXCEPTION_UNDEFINED();				\
				return;						\
			}							\
			offset = shift_mem(opcode);				\
		}								\
		else								\
			offset = opcode & 0xFFF;				\
		if (!(op & 0x8))						\
			offset = -offset;					\
		if (op & 0x10)							\
			addr += offset;						\
		CHECK_ADDR_EXCEPTION(addr);					\
		if ((op & 0x12) == 0x02)					\
			memmode = osmode ? MEMMODE_OS : MEMMODE_USER;		\
		if (!(op & 0x04))						\
		{								\
			templ = readmeml(addr);					\
			templ = ldrresult(templ, addr);				\
		}								\
		else								\
			templ = readmemb(addr);					\
		if ((op & 0x12) == 0x02)					\
			memmode = old_memmode;					\
		if (!databort)							\
		{								\
			cache_read_timing(addr, 1, 0);				\
			if (!(op & 0x10))					\
				armregs[RN] = addr + offset;			\
			else if (op & 0x2)					\
				armregs[RN] = addr;				\
			LOADREG(RD, templ);					\
			merge_timing(PC + 4);					\
		}								\
	}

opLDR(41)
opLDR(43)
opLDR(45)
opLDR(47)
opLDR(49)
opLDR(4b)
opLDR(4d)
opLDR(4f)
opLDR(51)
opLDR(53)
opLDR(55)
opLDR(57)
opLDR(59)
opLDR(5b)
opLDR(5d)
opLDR(5f)
opLDR(61)
opLDR(63)
opLDR(65)
opLDR(67)
opLDR(69)
opLDR(6b)
opLDR(6d)
opLDR(6f)
opLDR(71)
opLDR(73)
opLDR(75)
opLDR(77)
opLDR(79)
opLDR(7b)
opLDR(7d)
opLDR(7f)

#define opSTR(_op)								\
	static void opSTR##_op(uint32_t opcode)					\
	{									\
		uint32_t addr, offset;						\
		uint32_t templ;							\
		int old_memmode = memmode;					\
		const uint8_t op = 0x##_op;					\
										\
		addr = GETADDR(RN);						\
		if (op & 0x20)							\
		{								\
			if (opcode & 0x10) /*Shift by register*/		\
			{							\
				EXCEPTION_UNDEFINED();				\
				return;						\
			}							\
			offset = shift_mem(opcode);				\
		}								\
		else								\
			offset = opcode & 0xFFF;				\
		if (!(op & 0x8))						\
			offset = -offset;					\
		if (op & 0x10)							\
			addr += offset;						\
		CHECK_ADDR_EXCEPTION(addr);					\
		templ = (RD != 15) ? armregs[RD] : armregs[RD] + 4;		\
		if ((op & 0x12) == 0x02)					\
			memmode = osmode ? MEMMODE_OS : MEMMODE_USER;		\
		if (!(op & 0x04))						\
			writememl(addr, templ);					\
		else								\
			writememb(addr, templ);					\
		if ((op & 0x12) == 0x02)					\
			memmode = old_memmode;					\
		if (!databort)							\
		{								\
			cache_write_timing(addr, 1);				\
			if (!(op & 0x10))					\
				armregs[RN] = addr + offset;			\
			else if (op & 0x2)					\
				armregs[RN] = addr;				\
			promote_fetch_to_n = PROMOTE_NOMERGE;			\
		}								\
	}

opSTR(40)
opSTR(42)
opSTR(44)
opSTR(46)
opSTR(48)
opSTR(4a)
opSTR(4c)
opSTR(4e)
opSTR(50)
opSTR(52)
opSTR(54)
opSTR(56)
opSTR(58)
opSTR(5a)
opSTR(5c)
opSTR(5e)
opSTR(60)
opSTR(62)
opSTR(64)
opSTR(66)
opSTR(68)
opSTR(6a)
opSTR(6c)
opSTR(6e)
opSTR(70)
opSTR(72)
opSTR(74)
opSTR(76)
opSTR(78)
opSTR(7a)
opSTR(7c)
opSTR(7e)

#define STMfirst()      int c; \
			mask=1; \
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

#define STMfirstS()     int c; \
			mask = 1; \
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
			int first_access = 1; \
			for (int c = 0; c < 15; c++) \
			{ \
				if (opcode & mask) \
				{ \
					uint32_t templ = readmeml(addr); if (!databort) armregs[c] = templ; \
					cache_read_timing(addr, first_access || !(addr & 0xc), 0); \
					first_access = 0; \
					addr = (addr + 4) & 0x3fffffc; \
				} \
				mask<<=1; \
			} \
			if (opcode & 0x8000) \
			{ \
				uint32_t templ = readmeml(addr); \
				cache_read_timing(addr, first_access || !(addr & 0xc), 0); \
				if (!databort) armregs[15] = (armregs[15] & 0xFC000003) | ((templ+4) & 0x3FFFFFC); \
				refillpipeline(); \
			}

#define LDMallS()       mask = 1; \
			CHECK_ADDR_EXCEPTION(addr); \
			int first_access = 1; \
			if (opcode & 0x8000) \
			{ \
				for (int c = 0; c < 15; c++) \
				{ \
					if (opcode & mask) \
					{ \
						uint32_t templ = readmeml(addr); if (!databort) armregs[c] = templ; \
						cache_read_timing(addr, first_access || !(addr & 0xc), 0); \
						first_access = 0; \
						addr = (addr + 4) & 0x3fffffc; \
					} \
					mask <<= 1; \
				} \
				uint32_t templ = readmeml(addr); \
				cache_read_timing(addr, first_access || !(addr & 0xc), 0); \
				if (!databort) \
				{ \
					if (armregs[15] & 3) \
					{ \
						armregs[15] = (templ + 4); \
						if ((armregs[15] & 3) != mode) \
							updatemode(armregs[15] & 3); \
					} \
					else \
						armregs[15] = (armregs[15] & 0x0C000003) | ((templ + 4) & 0xF3FFFFFC); \
				} \
				refillpipeline(); \
			} \
			else \
			{ \
				for (int c = 0; c < 15; c++) \
				{ \
					if (opcode & mask) \
					{ \
						uint32_t templ = readmeml(addr); if (!databort) *usrregs[c] = templ; \
						cache_read_timing(addr, first_access || !(addr & 0xc), 0); \
						first_access = 0; \
						addr = (addr + 4) & 0x3fffffc; \
					} \
					mask <<= 1; \
				} \
			}

static void opSTMD(uint32_t opcode)
{
	uint32_t addr = armregs[RN] - countbits(opcode & 0xFFFF);
	uint32_t mask;

	if (!(opcode & 0x1000000))
		addr += 4;
	STMfirst();
	if ((opcode & 0x200000) && (RN != 15))
		armregs[RN] -= countbits(opcode & 0xFFFF);
	STMall()
	promote_fetch_to_n = PROMOTE_NOMERGE;
}

static void opSTMI(uint32_t opcode)
{
	uint32_t addr = armregs[RN];
	uint32_t mask;

	if (opcode & 0x1000000)
		addr += 4;
	STMfirst();
	if ((opcode & 0x200000) && (RN != 15))
		armregs[RN] += countbits(opcode & 0xFFFF);
	STMall();
	promote_fetch_to_n = PROMOTE_NOMERGE;
}

static void opSTMDS(uint32_t opcode)
{
	uint32_t addr = armregs[RN] - countbits(opcode & 0xFFFF);
	uint32_t mask;

	if (!(opcode & 0x1000000))
		addr += 4;
	STMfirstS();
	if ((opcode & 0x200000) && (RN != 15))
		armregs[RN] -= countbits(opcode & 0xFFFF);
	STMallS()
	promote_fetch_to_n = PROMOTE_NOMERGE;
}

static void opSTMIS(uint32_t opcode)
{
	uint32_t addr = armregs[RN];
	uint32_t mask;

	if (opcode & 0x1000000)
		addr += 4;
	STMfirstS();
	if ((opcode & 0x200000) && (RN != 15))
		armregs[RN]+=countbits(opcode&0xFFFF);
	STMallS();
	promote_fetch_to_n = PROMOTE_NOMERGE;
}

static void opLDMD(uint32_t opcode)
{
	uint32_t addr = armregs[RN] - countbits(opcode & 0xFFFF);
	uint32_t mask;

	if (!(opcode & 0x1000000))
		addr += 4;
	if ((opcode & 0x200000) && (RN != 15))
		armregs[RN] -= countbits(opcode & 0xFFFF);
	LDMall();
	merge_timing(PC+4);
}

static void opLDMI(uint32_t opcode)
{
	uint32_t addr = armregs[RN];
	uint32_t mask;

	if (opcode & 0x1000000)
		addr+=4;
	if ((opcode & 0x200000) && (RN != 15))
		armregs[RN]+=countbits(opcode & 0xFFFF);
	LDMall();
	merge_timing(PC+4);
}

static void opLDMDS(uint32_t opcode)
{
	uint32_t addr = armregs[RN] - countbits(opcode & 0xFFFF);
	uint32_t mask;

	if (!(opcode & 0x1000000))
		addr += 4;
	if ((opcode & 0x200000) && (RN != 15))
		armregs[RN] -= countbits(opcode & 0xFFFF);
	LDMallS();
	merge_timing(PC+4);
}

static void opLDMIS(uint32_t opcode)
{
	uint32_t addr = armregs[RN];
	uint32_t mask;

	if (opcode & 0x1000000)
		addr += 4;
	if ((opcode & 0x200000) && (RN != 15))
		armregs[RN] += countbits(opcode & 0xFFFF);
	LDMallS();
	merge_timing(PC+4);
}

static void opB(uint32_t opcode)
{
	uint32_t templ = (opcode & 0xFFFFFF) << 2;
	armregs[15] = ((armregs[15] + templ + 4) & 0x3FFFFFC) | (armregs[15] & 0xFC000003);
	refillpipeline();
}

static void opBL(uint32_t opcode)
{
	uint32_t templ = (opcode & 0xFFFFFF) << 2;
	armregs[14] = armregs[15] - 4;
	armregs[15] = ((armregs[15] + templ + 4) & 0x3FFFFFC) | (armregs[15] & 0xFC000003);
	refillpipeline();
}

static void opcopro(uint32_t opcode)
{
	if (((opcode & 0xF00) == 0x100 || (opcode & 0xF00) == 0x200) && fpaena)
	{
		if (fpaopcode(opcode))
			EXCEPTION_UNDEFINED();
		else
			promote_fetch_to_n = PROMOTE_NOMERGE;
	}
	else
		EXCEPTION_UNDEFINED();
}

static void opMCR(uint32_t opcode)
{
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
}

static void opMRC(uint32_t opcode)
{
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
}

static void opSWI(uint32_t opcode)
{
	if (debugon)
		debug_trap(DEBUG_TRAP_SWI, opcode);

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

	if ((opcode & 0xdffff) == ARCEM_SWI_HOSTFS)
	{
		ARMul_State state;

		state.Reg = armregs;
		uint32_t templ = memmode;
		memmode = MEMMODE_SUPER;
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
}

static const OpFn opcode_fns[256] =
{
/*00*/	opANDreg,	opANDregS,	opEORreg,	opEORregS,	opSUBreg,	opSUBregS,	opRSBreg,	opRSBregS,
/*08*/	opADDreg,	opADDregS,	opADCreg,	opADCregS,	opSBCreg,	opSBCregS,	opRSCreg,	opRSCregS,
/*10*/	opSWP,		opTSTreg,	opNULL,		opTEQreg,	opSWPB,		opCMPreg,	opNULL,		opCMNreg,
/*18*/	opORRreg,	opORRregS,	opMOVreg,	opMOVregS,	opBICreg,	opBICregS,	opMVNreg,	opMVNregS,
/*20*/	opANDimm,	opANDimmS,	opEORimm,	opEORimmS,	opSUBimm,	opSUBimmS,	opRSBimm,	opRSBimmS,
/*28*/	opADDimm,	opADDimmS,	opADCimm,	opADCimmS,	opSBCimm,	opSBCimmS,	opRSCimm,	opRSCimmS,
/*30*/	opUNDEF,	opTSTimm,	opNULL,		opTEQimm,	opUNDEF,	opCMPimm,	opNULL,		opCMNimm,
/*38*/	opORRimm,	opORRimmS,	opMOVimm,	opMOVimmS,	opBICimm,	opBICimmS,	opMVNimm,	opMVNimmS,

/*40*/	opSTR40,	opLDR41,	opSTR42,	opLDR43,	opSTR44,	opLDR45,	opSTR46,	opLDR47,
/*48*/	opSTR48,	opLDR49,	opSTR4a,	opLDR4b,	opSTR4c,	opLDR4d,	opSTR4e,	opLDR4f,
/*50*/	opSTR50,	opLDR51,	opSTR52,	opLDR53,	opSTR54,	opLDR55,	opSTR56,	opLDR57,
/*58*/	opSTR58,	opLDR59,	opSTR5a,	opLDR5b,	opSTR5c,	opLDR5d,	opSTR5e,	opLDR5f,
/*60*/	opSTR60,	opLDR61,	opSTR62,	opLDR63,	opSTR64,	opLDR65,	opSTR66,	opLDR67,
/*68*/	opSTR68,	opLDR69,	opSTR6a,	opLDR6b,	opSTR6c,	opLDR6d,	opSTR6e,	opLDR6f,
/*70*/	opSTR70,	opLDR71,	opSTR72,	opLDR73,	opSTR74,	opLDR75,	opSTR76,	opLDR77,
/*78*/	opSTR78,	opLDR79,	opSTR7a,	opLDR7b,	opSTR7c,	opLDR7d,	opSTR7e,	opLDR7f,

/*80*/	opSTMD,		opLDMD,		opSTMD,		opLDMD,		opSTMDS,	opLDMDS,	opSTMDS,	opLDMDS,
/*88*/	opSTMI,		opLDMI,		opSTMI,		opLDMI,		opSTMIS,	opLDMIS,	opSTMIS,	opLDMIS,
/*90*/	opSTMD,		opLDMD,		opSTMD,		opLDMD,		opSTMDS,	opLDMDS,	opSTMDS,	opLDMDS,
/*98*/	opSTMI,		opLDMI,		opSTMI,		opLDMI,		opSTMIS,	opLDMIS,	opSTMIS,	opLDMIS,
/*a0*/	opB,		opB,		opB,		opB,		opB,		opB,		opB,		opB,
/*a8*/	opB,		opB,		opB,		opB,		opB,		opB,		opB,		opB,
/*b0*/	opBL,		opBL,		opBL,		opBL,		opBL,		opBL,		opBL,		opBL,
/*b8*/	opBL,		opBL,		opBL,		opBL,		opBL,		opBL,		opBL,		opBL,

/*c0*/	opcopro,	opcopro,	opcopro,	opcopro,	opcopro,	opcopro,	opcopro,	opcopro,
/*c8*/	opcopro,	opcopro,	opcopro,	opcopro,	opcopro,	opcopro,	opcopro,	opcopro,
/*d0*/	opcopro,	opcopro,	opcopro,	opcopro,	opcopro,	opcopro,	opcopro,	opcopro,
/*d8*/	opcopro,	opcopro,	opcopro,	opcopro,	opcopro,	opcopro,	opcopro,	opcopro,
/*e0*/	opMCR,		opMRC,		opMCR,		opMRC,		opMCR,		opMRC,		opMCR,		opMRC,
/*e8*/	opMCR,		opMRC,		opMCR,		opMRC,		opMCR,		opMRC,		opMCR,		opMRC,
/*f0*/	opSWI,		opSWI,		opSWI,		opSWI,		opSWI,		opSWI,		opSWI,		opSWI,
/*f8*/	opSWI,		opSWI,		opSWI,		opSWI,		opSWI,		opSWI,		opSWI,		opSWI
};

/*Execute ARM instructions for `cycs` clock ticks, typically 10 ms
  (cycs=80k for an 8MHz ARM2).*/
void execarm(int cycles_to_execute)
{
	uint32_t templ,templ2;

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
				pccache2 = (uint32_t *)mempoint[templ2];
				opcode3 = pccache2[(PC & 0xFFF) >> 2];
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

		if (!prefabort)
		{
			if (debugon)
				debugger_do();

			if (flaglookup[opcode >> 28][armregs[15] >> 28])
				opcode_fns[(opcode >> 20) & 0xff](opcode);
		}

		if (databort|armirq|prefabort)
		{
			if (prefabort)       /*Prefetch abort*/
			{
				if (debugon)
					debug_trap(DEBUG_TRAP_PREF_ABORT, opcode);
				prefabort = 0;
				EXCEPTION_PREF_ABORT();
			}
			else if (databort == 1)     /*Data abort*/
			{
				if (debugon)
					debug_trap(DEBUG_TRAP_DATA_ABORT, opcode);
				databort = 0;
				EXCEPTION_DATA_ABORT();
			}
			else if (databort == 2) /*Address Exception*/
			{
				if (debugon)
					debug_trap(DEBUG_TRAP_ADDR_EXCEP, opcode);
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
#ifndef RELEASE_BUILD
		if ((armregs[15] & 3) != mode)
		{
			dumpregs();
			fatal("Mode mismatch\n");
		}

		if (output)
		{
			rpclog("%05i : %07X %08X %08X %08X %08X %08X %08X %08X %08X",ins,PC-8,armregs[0],armregs[1],armregs[2],armregs[3],armregs[4],armregs[5],armregs[6],armregs[7]);
			rpclog("  %08X %08X %08X %08X %08X %08X %08X %08X  %08X  %02X %02X %02X  %02X %02X %02X  %i %i\n",armregs[8],armregs[9],armregs[10],armregs[11],armregs[12],armregs[13],armregs[14],armregs[15],opcode,ioc.mska,ioc.mskb,ioc.mskf,ioc.irqa,ioc.irqb,ioc.fiq,  0, motoron);

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
		ins++;
#endif

		if (TIMER_VAL_LESS_THAN_VAL(timer_target, tsc >> 32))
			timer_process();

		total_cycles -= (tsc - oldcyc);

	}
	LOG_EVENT_LOOP("execarm() finished; and called pollline() %d times (should be ~160)\n",
		pollline_call_count);
}
