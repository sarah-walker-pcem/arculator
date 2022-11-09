#include "ibm.h"
#include "cpu.h"
#include "diva.h"
//#include "model.h"
#include "io.h"
#include "x86_ops.h"
#include "mem.h"
//#include "pci.h"
//#include "codegen.h"
#include "x87_timings.h"

int hasfpu;
int fpu_type;
uint32_t cpu_features;

static int cpu_turbo_speed, cpu_nonturbo_speed;
static int cpu_turbo = 1;

int cpuspeed;

int isa_cycles;
int has_vlb;
static uint8_t ccr0, ccr1, ccr2, ccr3, ccr4, ccr5, ccr6;

int cpu_pccard_cpu;

/*OpFn *x86_dynarec_opcodes;
OpFn *x86_dynarec_opcodes_0f;
OpFn *x86_dynarec_opcodes_d8_a16;
OpFn *x86_dynarec_opcodes_d8_a32;
OpFn *x86_dynarec_opcodes_d9_a16;
OpFn *x86_dynarec_opcodes_d9_a32;
OpFn *x86_dynarec_opcodes_da_a16;
OpFn *x86_dynarec_opcodes_da_a32;
OpFn *x86_dynarec_opcodes_db_a16;
OpFn *x86_dynarec_opcodes_db_a32;
OpFn *x86_dynarec_opcodes_dc_a16;
OpFn *x86_dynarec_opcodes_dc_a32;
OpFn *x86_dynarec_opcodes_dd_a16;
OpFn *x86_dynarec_opcodes_dd_a32;
OpFn *x86_dynarec_opcodes_de_a16;
OpFn *x86_dynarec_opcodes_de_a32;
OpFn *x86_dynarec_opcodes_df_a16;
OpFn *x86_dynarec_opcodes_df_a32;
OpFn *x86_dynarec_opcodes_REPE;
OpFn *x86_dynarec_opcodes_REPNE;
OpFn *x86_dynarec_opcodes_3DNOW;*/

OpFn *x86_opcodes;
OpFn *x86_opcodes_0f;
OpFn *x86_opcodes_d8_a16;
OpFn *x86_opcodes_d8_a32;
OpFn *x86_opcodes_d9_a16;
OpFn *x86_opcodes_d9_a32;
OpFn *x86_opcodes_da_a16;
OpFn *x86_opcodes_da_a32;
OpFn *x86_opcodes_db_a16;
OpFn *x86_opcodes_db_a32;
OpFn *x86_opcodes_dc_a16;
OpFn *x86_opcodes_dc_a32;
OpFn *x86_opcodes_dd_a16;
OpFn *x86_opcodes_dd_a32;
OpFn *x86_opcodes_de_a16;
OpFn *x86_opcodes_de_a32;
OpFn *x86_opcodes_df_a16;
OpFn *x86_opcodes_df_a32;
OpFn *x86_opcodes_REPE;
OpFn *x86_opcodes_REPNE;
OpFn *x86_opcodes_3DNOW;

enum
{
	CPUID_FPU = (1 << 0),
	CPUID_VME = (1 << 1),
	CPUID_PSE = (1 << 3),
	CPUID_TSC = (1 << 4),
	CPUID_MSR = (1 << 5),
	CPUID_CMPXCHG8B = (1 << 8),
	CPUID_SEP = (1 << 11),
	CPUID_CMOV = (1 << 15),
	CPUID_MMX = (1 << 23)
};

/*Addition flags returned by CPUID function 0x80000001*/
enum
{
	CPUID_3DNOW = (1 << 31)
};

int cpu = 3, cpu_manufacturer = 0;
CPU *cpu_s;
int cpu_multi;
int cpu_iscyrix;
int cpu_16bitbus;
int cpu_busspeed;
int cpu_use_dynarec;
int cpu_cyrix_alignment;

uint64_t cpu_CR4_mask;

int cpu_cycles_read, cpu_cycles_read_l, cpu_cycles_write, cpu_cycles_write_l;
int cpu_prefetch_cycles, cpu_prefetch_width, cpu_mem_prefetch_cycles, cpu_rom_prefetch_cycles;
int cpu_waitstates;
int cpu_cache_int_enabled, cpu_cache_ext_enabled;

int cpu_is_486;

int is386;
int is486;
int CPUID;

uint64_t tsc = 0;

int timing_rr;
int timing_mr, timing_mrl;
int timing_rm, timing_rml;
int timing_mm, timing_mml;
int timing_bt, timing_bnt;
int timing_int, timing_int_rm, timing_int_v86, timing_int_pm, timing_int_pm_outer;
int timing_iret_rm, timing_iret_v86, timing_iret_pm, timing_iret_pm_outer;
int timing_call_rm, timing_call_pm, timing_call_pm_gate, timing_call_pm_gate_inner;
int timing_retf_rm, timing_retf_pm, timing_retf_pm_outer;
int timing_jmp_rm, timing_jmp_pm, timing_jmp_pm_gate;
int timing_misaligned;

static struct
{
	uint32_t tr1, tr12;
	uint32_t cesr;
	uint32_t fcr;
	uint64_t fcr2, fcr3;
} msr;

void cpu_set_edx()
{
	EDX = 0x2308;//models[model].cpu[cpu_manufacturer].cpus[cpu].edx_reset;
}

int fpu_get_type(int model, int manu, int cpu, const char *internal_name)
{
	return FPU_NONE;
/*        CPU *cpu_s = &models[model].cpu[manu].cpus[cpu];
	const FPU *fpus = cpu_s->fpus;
	int fpu_type = fpus[0].type;
	int c = 0;

	while (fpus[c].internal_name)
	{
		if (!strcmp(internal_name, fpus[c].internal_name))
			fpu_type = fpus[c].type;
		c++;
	}

	return fpu_type;*/
}

const char *fpu_get_internal_name(int model, int manu, int cpu, int type)
{
	return "none";
/*        CPU *cpu_s = &models[model].cpu[manu].cpus[cpu];
	const FPU *fpus = cpu_s->fpus;
	int c = 0;

	while (fpus[c].internal_name)
	{
		if (fpus[c].type == type)
			return fpus[c].internal_name;
		c++;
	}

	return fpus[0].internal_name;*/
}

const char *fpu_get_name_from_index(int model, int manu, int cpu, int c)
{
	/*CPU *cpu_s = &models[model].cpu[manu].cpus[cpu];
	const FPU *fpus = cpu_s->fpus;

	return fpus[c].name;*/
	return "none";
}

int fpu_get_type_from_index(int model, int manu, int cpu, int c)
{
/*        CPU *cpu_s = &models[model].cpu[manu].cpus[cpu];
	const FPU *fpus = cpu_s->fpus;

	return fpus[c].type;*/
	return FPU_NONE;
}

CPU *cpu_s;

void cpu_set(int cpu_type, int fpu_present)
{
	cpu_pccard_cpu = cpu_type;
	//cpu_is_486 = is_486;
//        if (!models[model].cpu[cpu_manufacturer].cpus)
//        {
		/*CPU is invalid, set to default*/
		cpu_manufacturer = 0;
		cpu = 0;
//        }

	switch (cpu_type)
	{
		case PCCARD_CPU_386SX:
		cpu_s = &cpus_i386SX[2];
		break;
		case PCCARD_CPU_486SLC:
		cpu_s = &cpus_486SLC[1];
		break;
		case PCCARD_CPU_486SXLC2:
		cpu_s = &cpus_486SLC[5]; //&models[model].cpu[cpu_manufacturer].cpus[cpu];
		break;
	}

	CPUID    = cpu_s->cpuid_model;
	cpuspeed = cpu_s->speed;
	is8086   = (cpu_s->cpu_type > CPU_8088);
	is386    = (cpu_s->cpu_type >= CPU_386SX);
	is486    = (cpu_s->cpu_type >= CPU_i486SX) || (cpu_s->cpu_type == CPU_486SLC || cpu_s->cpu_type == CPU_486DLC);
	hasfpu   = fpu_present;//(fpu_type != FPU_NONE);

	 cpu_iscyrix = (cpu_s->cpu_type == CPU_486SLC || cpu_s->cpu_type == CPU_486DLC || cpu_s->cpu_type == CPU_Cx486S || cpu_s->cpu_type == CPU_Cx486DX || cpu_s->cpu_type == CPU_Cx5x86 || cpu_s->cpu_type == CPU_Cx6x86 || cpu_s->cpu_type == CPU_Cx6x86MX || cpu_s->cpu_type == CPU_Cx6x86L || cpu_s->cpu_type == CPU_CxGX1);
	cpu_16bitbus = (cpu_s->cpu_type == CPU_286 || cpu_s->cpu_type == CPU_386SX || cpu_s->cpu_type == CPU_486SLC);
	if (cpu_s->multi)
	   cpu_busspeed = cpu_s->rspeed / cpu_s->multi;
	cpu_multi = cpu_s->multi;
	ccr0 = ccr1 = ccr2 = ccr3 = ccr4 = ccr5 = ccr6 = 0;
	has_vlb = (cpu_s->cpu_type >= CPU_i486SX) && (cpu_s->cpu_type <= CPU_Cx5x86);

	cpu_turbo_speed = cpu_s->rspeed;
	if (cpu_s->cpu_type < CPU_286)
		cpu_nonturbo_speed = 4772728;
	else if (cpu_s->rspeed < 8000000)
		cpu_nonturbo_speed = cpu_s->rspeed;
	else
		cpu_nonturbo_speed = 8000000;
	cpu_turbo = 1;

	cpu_update_waitstates();

	isa_cycles = cpu_s->atclk_div;

	if (cpu_s->rspeed <= 8000000)
		cpu_rom_prefetch_cycles = cpu_mem_prefetch_cycles;
	else
		cpu_rom_prefetch_cycles = cpu_s->rspeed / 1000000;

/*        if (cpu_s->pci_speed)
	{
		pci_nonburst_time = 4*cpu_s->rspeed / cpu_s->pci_speed;
		pci_burst_time = cpu_s->rspeed / cpu_s->pci_speed;
	}
	else
	{
		pci_nonburst_time = 4;
		pci_burst_time = 1;
	}
	pclog("PCI burst=%i nonburst=%i\n", pci_burst_time, pci_nonburst_time);*/

	if (cpu_iscyrix)
	   io_sethandler(0x0022, 0x0002, cyrix_read, NULL, NULL, cyrix_write, NULL, NULL, NULL);
	else
	   io_removehandler(0x0022, 0x0002, cyrix_read, NULL, NULL, cyrix_write, NULL, NULL, NULL);

	pclog("hasfpu - %i\n",hasfpu);
	pclog("is486 - %i  %i\n",is486,cpu_s->cpu_type);

	x86_setopcodes(ops_386, ops_386_0f);
	x86_opcodes_REPE = ops_REPE;
	x86_opcodes_REPNE = ops_REPNE;

	/*if (hasfpu)
	{
		x86_dynarec_opcodes_d8_a16 = dynarec_ops_fpu_d8_a16;
		x86_dynarec_opcodes_d8_a32 = dynarec_ops_fpu_d8_a32;
		x86_dynarec_opcodes_d9_a16 = dynarec_ops_fpu_d9_a16;
		x86_dynarec_opcodes_d9_a32 = dynarec_ops_fpu_d9_a32;
		x86_dynarec_opcodes_da_a16 = dynarec_ops_fpu_da_a16;
		x86_dynarec_opcodes_da_a32 = dynarec_ops_fpu_da_a32;
		x86_dynarec_opcodes_db_a16 = dynarec_ops_fpu_db_a16;
		x86_dynarec_opcodes_db_a32 = dynarec_ops_fpu_db_a32;
		x86_dynarec_opcodes_dc_a16 = dynarec_ops_fpu_dc_a16;
		x86_dynarec_opcodes_dc_a32 = dynarec_ops_fpu_dc_a32;
		x86_dynarec_opcodes_dd_a16 = dynarec_ops_fpu_dd_a16;
		x86_dynarec_opcodes_dd_a32 = dynarec_ops_fpu_dd_a32;
		x86_dynarec_opcodes_de_a16 = dynarec_ops_fpu_de_a16;
		x86_dynarec_opcodes_de_a32 = dynarec_ops_fpu_de_a32;
		x86_dynarec_opcodes_df_a16 = dynarec_ops_fpu_df_a16;
		x86_dynarec_opcodes_df_a32 = dynarec_ops_fpu_df_a32;
	}
	else
	{
		x86_dynarec_opcodes_d8_a16 = dynarec_ops_nofpu_a16;
		x86_dynarec_opcodes_d8_a32 = dynarec_ops_nofpu_a32;
		x86_dynarec_opcodes_d9_a16 = dynarec_ops_nofpu_a16;
		x86_dynarec_opcodes_d9_a32 = dynarec_ops_nofpu_a32;
		x86_dynarec_opcodes_da_a16 = dynarec_ops_nofpu_a16;
		x86_dynarec_opcodes_da_a32 = dynarec_ops_nofpu_a32;
		x86_dynarec_opcodes_db_a16 = dynarec_ops_nofpu_a16;
		x86_dynarec_opcodes_db_a32 = dynarec_ops_nofpu_a32;
		x86_dynarec_opcodes_dc_a16 = dynarec_ops_nofpu_a16;
		x86_dynarec_opcodes_dc_a32 = dynarec_ops_nofpu_a32;
		x86_dynarec_opcodes_dd_a16 = dynarec_ops_nofpu_a16;
		x86_dynarec_opcodes_dd_a32 = dynarec_ops_nofpu_a32;
		x86_dynarec_opcodes_de_a16 = dynarec_ops_nofpu_a16;
		x86_dynarec_opcodes_de_a32 = dynarec_ops_nofpu_a32;
		x86_dynarec_opcodes_df_a16 = dynarec_ops_nofpu_a16;
		x86_dynarec_opcodes_df_a32 = dynarec_ops_nofpu_a32;
	}*/
	//codegen_timing_set(&codegen_timing_486);

	if (hasfpu)
	{
		x86_opcodes_d8_a16 = ops_fpu_d8_a16;
		x86_opcodes_d8_a32 = ops_fpu_d8_a32;
		x86_opcodes_d9_a16 = ops_fpu_d9_a16;
		x86_opcodes_d9_a32 = ops_fpu_d9_a32;
		x86_opcodes_da_a16 = ops_fpu_da_a16;
		x86_opcodes_da_a32 = ops_fpu_da_a32;
		x86_opcodes_db_a16 = ops_fpu_db_a16;
		x86_opcodes_db_a32 = ops_fpu_db_a32;
		x86_opcodes_dc_a16 = ops_fpu_dc_a16;
		x86_opcodes_dc_a32 = ops_fpu_dc_a32;
		x86_opcodes_dd_a16 = ops_fpu_dd_a16;
		x86_opcodes_dd_a32 = ops_fpu_dd_a32;
		x86_opcodes_de_a16 = ops_fpu_de_a16;
		x86_opcodes_de_a32 = ops_fpu_de_a32;
		x86_opcodes_df_a16 = ops_fpu_df_a16;
		x86_opcodes_df_a32 = ops_fpu_df_a32;
	}
	else
	{
		x86_opcodes_d8_a16 = ops_nofpu_a16;
		x86_opcodes_d8_a32 = ops_nofpu_a32;
		x86_opcodes_d9_a16 = ops_nofpu_a16;
		x86_opcodes_d9_a32 = ops_nofpu_a32;
		x86_opcodes_da_a16 = ops_nofpu_a16;
		x86_opcodes_da_a32 = ops_nofpu_a32;
		x86_opcodes_db_a16 = ops_nofpu_a16;
		x86_opcodes_db_a32 = ops_nofpu_a32;
		x86_opcodes_dc_a16 = ops_nofpu_a16;
		x86_opcodes_dc_a32 = ops_nofpu_a32;
		x86_opcodes_dd_a16 = ops_nofpu_a16;
		x86_opcodes_dd_a32 = ops_nofpu_a32;
		x86_opcodes_de_a16 = ops_nofpu_a16;
		x86_opcodes_de_a32 = ops_nofpu_a32;
		x86_opcodes_df_a16 = ops_nofpu_a16;
		x86_opcodes_df_a32 = ops_nofpu_a32;
	}

	memset(&msr, 0, sizeof(msr));

	timing_misaligned = 0;
	cpu_cyrix_alignment = 0;
	cpu_CR4_mask = 0;

	switch (cpu_s->cpu_type)
	{
		case CPU_386SX:
		timing_rr  = 2;   /*register dest - register src*/
		timing_rm  = 6;   /*register dest - memory src*/
		timing_mr  = 7;   /*memory dest   - register src*/
		timing_mm  = 6;   /*memory dest   - memory src*/
		timing_rml = 8;   /*register dest - memory src long*/
		timing_mrl = 11;  /*memory dest   - register src long*/
		timing_mml = 10;  /*memory dest   - memory src*/
		timing_bt  = 7-3; /*branch taken*/
		timing_bnt = 3;   /*branch not taken*/
		timing_int = 0;
		timing_int_rm       = 37;
		timing_int_v86      = 59;
		timing_int_pm       = 99;
		timing_int_pm_outer = 119;
		timing_iret_rm       = 22;
		timing_iret_v86      = 60;
		timing_iret_pm       = 38;
		timing_iret_pm_outer = 82;
		timing_call_rm            = 17;
		timing_call_pm            = 34;
		timing_call_pm_gate       = 52;
		timing_call_pm_gate_inner = 86;
		timing_retf_rm       = 18;
		timing_retf_pm       = 32;
		timing_retf_pm_outer = 68;
		timing_jmp_rm      = 12;
		timing_jmp_pm      = 27;
		timing_jmp_pm_gate = 45;
		break;

		case CPU_486SLC:
		timing_rr  = 1; /*register dest - register src*/
		timing_rm  = 3; /*register dest - memory src*/
		timing_mr  = 5; /*memory dest   - register src*/
		timing_mm  = 3;
		timing_rml = 5; /*register dest - memory src long*/
		timing_mrl = 7; /*memory dest   - register src long*/
		timing_mml = 7;
		timing_bt  = 6-1; /*branch taken*/
		timing_bnt = 1; /*branch not taken*/
		/*unknown*/
		timing_int = 4;
		timing_int_rm       = 14;
		timing_int_v86      = 82;
		timing_int_pm       = 49;
		timing_int_pm_outer = 77;
		timing_iret_rm       = 14;
		timing_iret_v86      = 66;
		timing_iret_pm       = 31;
		timing_iret_pm_outer = 66;
		timing_call_rm = 12;
		timing_call_pm = 30;
		timing_call_pm_gate = 41;
		timing_call_pm_gate_inner = 83;
		timing_retf_rm       = 13;
		timing_retf_pm       = 26;
		timing_retf_pm_outer = 61;
		timing_jmp_rm      = 9;
		timing_jmp_pm      = 26;
		timing_jmp_pm_gate = 37;
		timing_misaligned = 3;
		break;

		default:
		fatal("cpu_set : unknown CPU type %i\n", cpu_s->cpu_type);
	}

	x87_timings = x87_timings_387;
}

void cpu_CPUID()
{
}

void cpu_RDMSR()
{
}

void cpu_WRMSR()
{
}

static int cyrix_addr;

#define CCR1_USE_SMI  (1 << 1)
#define CCR1_SMAC     (1 << 2)
#define CCR1_SM3      (1 << 7)

#define CCR3_SMI_LOCK (1 << 0)
#define CCR3_NMI_EN   (1 << 1)

void cyrix_write(uint16_t addr, uint8_t val, void *priv)
{
	if (!(addr & 1))
		cyrix_addr = val;
	else switch (cyrix_addr)
	{
		case 0xc0: /*CCR0*/
		ccr0 = val;
		break;
		case 0xc1: /*CCR1*/
		if ((ccr3 & CCR3_SMI_LOCK) && !(cpu_cur_status & CPU_STATUS_SMM))
			val = (val & ~(CCR1_USE_SMI | CCR1_SMAC | CCR1_SM3)) | (ccr1 & (CCR1_USE_SMI | CCR1_SMAC | CCR1_SM3));
		ccr1 = val;
		break;
		case 0xc2: /*CCR2*/
		ccr2 = val;
		break;
		case 0xc3: /*CCR3*/
		if ((ccr3 & CCR3_SMI_LOCK) && !(cpu_cur_status & CPU_STATUS_SMM))
			val = (val & ~(CCR3_NMI_EN)) | (ccr3 & CCR3_NMI_EN) | CCR3_SMI_LOCK;
		ccr3 = val;
		break;
		case 0xcd:
		if (!(ccr3 & CCR3_SMI_LOCK) || (cpu_cur_status & CPU_STATUS_SMM))
		{
			cyrix.arr[3].base = (cyrix.arr[3].base & ~0xff000000) | (val << 24);
			cyrix.smhr &= ~SMHR_VALID;
		}
		break;
		case 0xce:
		if (!(ccr3 & CCR3_SMI_LOCK) || (cpu_cur_status & CPU_STATUS_SMM))
		{
			cyrix.arr[3].base = (cyrix.arr[3].base & ~0x00ff0000) | (val << 16);
			cyrix.smhr &= ~SMHR_VALID;
		}
		break;
		case 0xcf:
		if (!(ccr3 & CCR3_SMI_LOCK) || (cpu_cur_status & CPU_STATUS_SMM))
		{
			cyrix.arr[3].base = (cyrix.arr[3].base & ~0x0000f000) | ((val & 0xf0) << 8);
			if ((val & 0xf) == 0xf)
				cyrix.arr[3].size = 1ull << 32; /*4 GB*/
			else if (val & 0xf)
				cyrix.arr[3].size = 2048 << (val & 0xf);
			else
				cyrix.arr[3].size = 0; /*Disabled*/
			cyrix.smhr &= ~SMHR_VALID;
		}
		break;

		case 0xe8: /*CCR4*/
		if ((ccr3 & 0xf0) == 0x10)
		{
			ccr4 = val;
		}
		break;
		case 0xe9: /*CCR5*/
		if ((ccr3 & 0xf0) == 0x10)
			ccr5 = val;
		break;
		case 0xea: /*CCR6*/
		if ((ccr3 & 0xf0) == 0x10)
			ccr6 = val;
		break;
	}
}

uint8_t cyrix_read(uint16_t addr, void *priv)
{
	if (addr & 1)
	{
		switch (cyrix_addr)
		{
			case 0xc0: return ccr0;
			case 0xc1: return ccr1;
			case 0xc2: return ccr2;
			case 0xc3: return ccr3;
			case 0xe8: return ((ccr3 & 0xf0) == 0x10) ? ccr4 : 0xff;
			case 0xe9: return ((ccr3 & 0xf0) == 0x10) ? ccr5 : 0xff;
			case 0xea: return ((ccr3 & 0xf0) == 0x10) ? ccr6 : 0xff;
			case 0xfe: return 0;//models[model].cpu[cpu_manufacturer].cpus[cpu].cyrix_id & 0xff;
			case 0xff: return 0;//models[model].cpu[cpu_manufacturer].cpus[cpu].cyrix_id >> 8;
		}
		//if (cyrix_addr == 0x20 && models[model].cpu[cpu_manufacturer].cpus[cpu].cpu_type == CPU_Cx5x86) return 0xff;
	}
	return 0xff;
}

void x86_setopcodes(OpFn *opcodes, OpFn *opcodes_0f)
{
	x86_opcodes = opcodes;
	x86_opcodes_0f = opcodes_0f;
}

void cpu_update_waitstates()
{
//	if (cpu_is_486)
//        	cpu_s = &cpus_486SLC[1];
//        else
//	        cpu_s = &cpus_i386SX[2]; //&models[model].cpu[cpu_manufacturer].cpus[cpu];

	if (is486)
		cpu_prefetch_width = 16;
	else
		cpu_prefetch_width = cpu_16bitbus ? 2 : 4;

	if (cpu_cache_int_enabled)
	{
		/* Disable prefetch emulation */
		cpu_prefetch_cycles = 0;
	}
	else if (cpu_waitstates && (cpu_s->cpu_type >= CPU_286 && cpu_s->cpu_type <= CPU_386DX))
	{
		/* Waitstates override */
		cpu_prefetch_cycles = cpu_waitstates+1;
		cpu_cycles_read = cpu_waitstates+1;
		cpu_cycles_read_l = (cpu_16bitbus ? 2 : 1) * (cpu_waitstates+1);
		cpu_cycles_write = cpu_waitstates+1;
		cpu_cycles_write_l = (cpu_16bitbus ? 2 : 1) * (cpu_waitstates+1);
	}
	else if (cpu_cache_ext_enabled)
	{
		/* Use cache timings */
		cpu_prefetch_cycles = cpu_s->cache_read_cycles;
		cpu_cycles_read = cpu_s->cache_read_cycles;
		cpu_cycles_read_l = (cpu_16bitbus ? 2 : 1) * cpu_s->cache_read_cycles;
		cpu_cycles_write = cpu_s->cache_write_cycles;
		cpu_cycles_write_l = (cpu_16bitbus ? 2 : 1) * cpu_s->cache_write_cycles;
	}
	else
	{
		/* Use memory timings */
		cpu_prefetch_cycles = cpu_s->mem_read_cycles;
		cpu_cycles_read = cpu_s->mem_read_cycles;
		cpu_cycles_read_l = (cpu_16bitbus ? 2 : 1) * cpu_s->mem_read_cycles;
		cpu_cycles_write = cpu_s->mem_write_cycles;
		cpu_cycles_write_l = (cpu_16bitbus ? 2 : 1) * cpu_s->mem_write_cycles;
	}
	if (is486)
		cpu_prefetch_cycles = (cpu_prefetch_cycles * 11) / 16;
	cpu_mem_prefetch_cycles = cpu_prefetch_cycles;
	if (cpu_s->rspeed <= 8000000)
		cpu_rom_prefetch_cycles = cpu_mem_prefetch_cycles;
}

void cpu_set_turbo(int turbo)
{
	if (cpu_turbo != turbo)
	{
		cpu_turbo = turbo;

//		if (cpu_is_486)
//        		cpu_s = &cpus_486SLC[1];
//        	else
//		        cpu_s = &cpus_i386SX[2]; //&models[model].cpu[cpu_manufacturer].cpus[cpu];
		if (cpu_s->cpu_type >= CPU_286)
		{
			if (cpu_turbo)
				setpitclock(cpu_turbo_speed);
			else
				setpitclock(cpu_nonturbo_speed);
		}
		else
			setpitclock(14318184.0);
	}
}

int cpu_get_turbo()
{
	return cpu_turbo;
}

int cpu_get_speed()
{
	if (cpu_turbo)
		return cpu_turbo_speed;
	return cpu_nonturbo_speed;
}

void cpu_set_nonturbo_divider(int divider)
{
	if (divider < 2)
		cpu_set_turbo(1);
	else
	{
		cpu_nonturbo_speed = cpu_turbo_speed / divider;
		cpu_set_turbo(0);
	}
}
