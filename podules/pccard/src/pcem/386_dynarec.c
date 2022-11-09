#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#if defined(__APPLE__) && defined(__aarch64__)
#include <pthread.h>
#endif
#include "ibm.h"
#include "x86.h"
#include "x86_ops.h"
#include "x87.h"
#include "mem.h"
#include "cpu.h"
#include "pic.h"
#include "timer.h"

#include "386_common.h"

#define CPU_BLOCK_END() cpu_block_end = 1

static int codegen_flags_changed;
int cpu_end_block_after_ins;

int inrecomp = 0;
int cpu_recomp_blocks, cpu_recomp_full_ins, cpu_new_blocks;
int cpu_recomp_blocks_latched, cpu_recomp_ins_latched, cpu_recomp_full_ins_latched, cpu_new_blocks_latched;

int cpu_block_end = 0;



static inline void fetch_ea_32_long(uint32_t rmdat)
{
	eal_r = eal_w = NULL;
	easeg = cpu_state.ea_seg->base;
	if (cpu_rm == 4)
	{
		uint8_t sib = rmdat >> 8;

		switch (cpu_mod)
		{
			case 0:
			cpu_state.eaaddr = cpu_state.regs[sib & 7].l;
			cpu_state.pc++;
			break;
			case 1:
			cpu_state.pc++;
			cpu_state.eaaddr = ((uint32_t)(int8_t)getbyte()) + cpu_state.regs[sib & 7].l;
//                        cpu_state.pc++;
			break;
			case 2:
			cpu_state.eaaddr = (fastreadl(cs + cpu_state.pc + 1)) + cpu_state.regs[sib & 7].l;
			cpu_state.pc += 5;
			break;
		}
		/*SIB byte present*/
		if ((sib & 7) == 5 && !cpu_mod)
			cpu_state.eaaddr = getlong();
		else if ((sib & 6) == 4 && !cpu_state.ssegs)
		{
			easeg = ss;
			cpu_state.ea_seg = &cpu_state.seg_ss;
		}
		if (((sib >> 3) & 7) != 4)
			cpu_state.eaaddr += cpu_state.regs[(sib >> 3) & 7].l << (sib >> 6);
	}
	else
	{
		cpu_state.eaaddr = cpu_state.regs[cpu_rm].l;
		if (cpu_mod)
		{
			if (cpu_rm == 5 && !cpu_state.ssegs)
			{
				easeg = ss;
				cpu_state.ea_seg = &cpu_state.seg_ss;
			}
			if (cpu_mod == 1)
			{
				cpu_state.eaaddr += ((uint32_t)(int8_t)(rmdat >> 8));
				cpu_state.pc++;
			}
			else
			{
				cpu_state.eaaddr += getlong();
			}
		}
		else if (cpu_rm == 5)
		{
			cpu_state.eaaddr = getlong();
		}
	}
	if (easeg != 0xFFFFFFFF && ((easeg + cpu_state.eaaddr) & 0xFFF) <= 0xFFC)
	{
		uint32_t addr = easeg + cpu_state.eaaddr;
		if ( readlookup2[addr >> 12] != -1)
		   eal_r = (uint32_t *)(readlookup2[addr >> 12] + addr);
		if (writelookup2[addr >> 12] != -1)
		   eal_w = (uint32_t *)(writelookup2[addr >> 12] + addr);
	}
}

static inline void fetch_ea_16_long(uint32_t rmdat)
{
	eal_r = eal_w = NULL;
	easeg = cpu_state.ea_seg->base;
	if (!cpu_mod && cpu_rm == 6)
	{
		cpu_state.eaaddr = getword();
	}
	else
	{
		switch (cpu_mod)
		{
			case 0:
			cpu_state.eaaddr = 0;
			break;
			case 1:
			cpu_state.eaaddr = (uint16_t)(int8_t)(rmdat >> 8); cpu_state.pc++;
			break;
			case 2:
			cpu_state.eaaddr = getword();
			break;
		}
		cpu_state.eaaddr += (*mod1add[0][cpu_rm]) + (*mod1add[1][cpu_rm]);
		if (mod1seg[cpu_rm] == &ss && !cpu_state.ssegs)
		{
			easeg = ss;
			cpu_state.ea_seg = &cpu_state.seg_ss;
		}
		cpu_state.eaaddr &= 0xFFFF;
	}
	if (easeg != 0xFFFFFFFF && ((easeg + cpu_state.eaaddr) & 0xFFF) <= 0xFFC)
	{
		uint32_t addr = easeg + cpu_state.eaaddr;
		if ( readlookup2[addr >> 12] != -1)
		   eal_r = (uint32_t *)(readlookup2[addr >> 12] + addr);
		if (writelookup2[addr >> 12] != -1)
		   eal_w = (uint32_t *)(writelookup2[addr >> 12] + addr);
	}
}

#define fetch_ea_16(rmdat)              cpu_state.pc++; cpu_mod=(rmdat >> 6) & 3; cpu_reg=(rmdat >> 3) & 7; cpu_rm = rmdat & 7; if (cpu_mod != 3) { fetch_ea_16_long(rmdat); if (cpu_state.abrt) return 1; }
#define fetch_ea_32(rmdat)              cpu_state.pc++; cpu_mod=(rmdat >> 6) & 3; cpu_reg=(rmdat >> 3) & 7; cpu_rm = rmdat & 7; if (cpu_mod != 3) { fetch_ea_32_long(rmdat); } if (cpu_state.abrt) return 1

#include "x86_flags.h"


/*Prefetch emulation is a fairly simplistic model:
  - All instruction bytes must be fetched before it starts.
  - Cycles used for non-instruction memory accesses are counted and subtracted
    from the total cycles taken
  - Any remaining cycles are used to refill the prefetch queue.

  Note that this is only used for 286 / 386 systems. It is disabled when the
  internal cache on 486+ CPUs is enabled.
*/
static int prefetch_bytes = 0;
static int prefetch_prefixes = 0;

static void prefetch_run(int instr_cycles, int bytes, int modrm, int reads, int reads_l, int writes, int writes_l, int ea32)
{
	int mem_cycles = reads*cpu_cycles_read + reads_l*cpu_cycles_read_l + writes*cpu_cycles_write + writes_l*cpu_cycles_write_l;

	if (instr_cycles < mem_cycles)
		instr_cycles = mem_cycles;

	prefetch_bytes -= prefetch_prefixes;
	prefetch_bytes -= bytes;
	if (modrm != -1)
	{
		if (ea32)
		{
			if ((modrm & 7) == 4)
			{
				if ((modrm & 0x700) == 0x500)
					prefetch_bytes -= 5;
				else if ((modrm & 0xc0) == 0x40)
					prefetch_bytes -= 2;
				else if ((modrm & 0xc0) == 0x80)
					prefetch_bytes -= 5;
			}
			else
			{
				if ((modrm & 0xc7) == 0x05)
					prefetch_bytes -= 4;
				else if ((modrm & 0xc0) == 0x40)
					prefetch_bytes--;
				else if ((modrm & 0xc0) == 0x80)
					prefetch_bytes -= 4;
			}
		}
		else
		{
			if ((modrm & 0xc7) == 0x06)
				prefetch_bytes -= 2;
			else if ((modrm & 0xc0) != 0xc0)
				prefetch_bytes -= ((modrm & 0xc0) >> 6);
		}
	}

	/* Fill up prefetch queue */
	while (prefetch_bytes < 0)
	{
		prefetch_bytes += cpu_prefetch_width;
		cycles -= cpu_prefetch_cycles;
	}

	/* Subtract cycles used for memory access by instruction */
	instr_cycles -= mem_cycles;

	while (instr_cycles >= cpu_prefetch_cycles)
	{
		prefetch_bytes += cpu_prefetch_width;
		instr_cycles -= cpu_prefetch_cycles;
	}

	prefetch_prefixes = 0;
	if (prefetch_bytes > 16)
		prefetch_bytes = 16;
}

static void prefetch_flush()
{
	prefetch_bytes = 0;
}

#define PREFETCH_RUN(instr_cycles, bytes, modrm, reads, reads_l, writes, writes_l, ea32) \
	do { if (cpu_prefetch_cycles) prefetch_run(instr_cycles, bytes, modrm, reads, reads_l, writes, writes_l, ea32); } while (0)

#define PREFETCH_PREFIX() do { if (cpu_prefetch_cycles) prefetch_prefixes++; } while (0)
#define PREFETCH_FLUSH() prefetch_flush()


#define OP_TABLE(name) ops_ ## name
#define CLOCK_CYCLES(c) cycles -= (c)
#define CLOCK_CYCLES_ALWAYS(c) cycles -= (c)

#include "386_ops.h"
