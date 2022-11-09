#include <string.h>
#include "arc.h"
#include "arm.h"
#include "debugger.h"
#include "debugger_swis.h"
#include "ioc.h"
#include "mem.h"
#include "memc.h"
#include "vidc.h"

void debug_start(void)
{
}

void debug_end(void)
{
	debug = debugon = 0;
}

void debug_kill(void)
{
}

void debug_out(char *s)
{
	rpclog("%s", s);
	console_output(s);
}


int debug = 0;
int debugon = 0;
int indebug = 0;
int debugger_in_reset = 0;

static uint32_t debug_memaddr=0;
static uint32_t debug_disaddr=0;
static char debug_lastcommand[256];

static int32_t breakpoints[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
static int debug_step_count = 0;
static uint32_t debug_trap_enable = 0;

#define RD ((opcode>>12)&0xF)
#define RN ((opcode>>16)&0xF)
#define RM (opcode&0xF)

#define MULRD ((opcode>>16)&0xF)
#define MULRN ((opcode>>12)&0xF)
#define MULRS ((opcode>>8)&0xF)
#define MULRM (opcode&0xF)

static const char *cpu_conditions[16] =
{
	"EQ", "NE", "CS", "CC", "MI", "PL", "VS", "VC",
	"HI", "LS", "GE", "LT", "GT", "LE", "", "NV"
};

static const char *dp_instructions[16] =
{
	"AND", "EOR", "SUB", "RSB", "ADD", "ADC", "SBC", "RSC",
	"TST", "TEQ", "CMP", "CMN", "ORR", "MOV", "BIC", "MVN"
};

static const char *shift_types[4] =
{
	"LSL", "LSR", "ASR", "ROR"
};

static void debug_print_shift(uint32_t opcode)
{
	int type = (opcode >> 5) & 3;
	char s[256];

	if (!(opcode & 0xff0)) /*LSL by 0, ie no shift*/
		return;

	if (opcode & (1 << 4)) /*Shift by register*/
	{
		sprintf(s, " %s R%i", shift_types[type], (opcode >> 8) & 0xf);
		debug_out(s);
	}
	else /*Shift by immediate*/
	{
		int shift_amount = (opcode >> 7) & 0x1f;

		if (type == 3 && !shift_amount)
			sprintf(s, " RRX");
		else
			sprintf(s, " %s #%i", shift_types[type],
				shift_amount ? shift_amount : 32);
		debug_out(s);
	}
}

static void debug_print_register_list(uint32_t opcode)
{
	int first_register = 1;
	int run_start = -1;
	char s[256];

	for (int i = 0; i < 16; i++)
	{
		if (opcode & (1 << i))
		{
			if (run_start == -1)
				run_start = i;
		}
		else
		{
			if (run_start != -1)
			{
				int run_length = i - run_start;

				if (!first_register)
					debug_out(", ");
				first_register = 0;

				if (run_length == 1)
					sprintf(s, "R%i", run_start);
				else
					sprintf(s, "R%i-R%i", run_start, i-1);
				debug_out(s);

				run_start = -1;
			}
		}
	}

	if (run_start != -1)
	{
		int run_length = 16 - run_start;

		if (!first_register)
			debug_out(", ");

		if (run_length == 1)
			sprintf(s, "R%i", run_start);
		else
			sprintf(s, "R%i-R%i", run_start, 15);
		debug_out(s);
	}
}

static void debug_disassemble(void)
{
	uint32_t opcode;
	const char *cond;
	char s[256];

	opcode = readmemf_debug(debug_disaddr);
	sprintf(s, "%07X : (%08x) ", debug_disaddr, opcode);
	debug_out(s);

	cond = cpu_conditions[opcode >> 28];

	if ((opcode & 0x0e000000) == 0x00000000) /*Data processing, shift*/
	{
		if ((opcode & 0x0fe000f0) == 0x00000090) /*MUL*/
			sprintf(s, "MUL%s%s R%i, R%i, R%i", cond,
				(opcode & (1 << 20)) ? "S" : "",
				MULRD, MULRM, MULRS);
		else if ((opcode & 0x0fe000f0) == 0x00200090) /*MLA*/
			sprintf(s, "MLA%s%s R%i, R%i, R%i, R%i", cond,
				(opcode & (1 << 20)) ? "S" : "",
				MULRD, MULRM, MULRS, MULRN);
		else if ((opcode & 0x01a00000) == 0x01a00000) /*MOV/MVN*/
			sprintf(s, "%s%s%s R%i, R%i",
				dp_instructions[(opcode >> 21) & 0xf], cond,
				(opcode & (1 << 20)) ? "S" : "",
				RD, RM);
		else if ((opcode & 0x0180f000) == 0x0100f000) /*TSTP/TEQP/CMPP/CMNP*/
			sprintf(s, "%s%sP R%i, R%i",
				dp_instructions[(opcode >> 21) & 0xf],
				cond, RN, RM);
		else if ((opcode & 0x01800000) == 0x01000000) /*TST/TEQ/CMP/CMN*/
			sprintf(s, "%s%s R%i, R%i",
				dp_instructions[(opcode >> 21) & 0xf],
				cond, RN, RM);
		else
			sprintf(s, "%s%s%s R%i, R%i, R%i",
				dp_instructions[(opcode >> 21) & 0xf], cond,
				(opcode & (1 << 20)) ? "S" : "",
				RD, RN, RM);

		debug_out(s);
		debug_print_shift(opcode);
	}
	else if ((opcode & 0x0e000000) == 0x02000000) /*Data processing, immediate*/
	{
		if ((opcode & 0x01a00000) == 0x01a00000) /*MOV/MVN*/
			sprintf(s, "%s%s%s R%i, #%x",
				dp_instructions[(opcode >> 21) & 0xf], cond,
				(opcode & (1 << 20)) ? "S" : "",
				RD, rotatelookup[opcode & 0xfff]);
		else if ((opcode & 0x0180f000) == 0x0100f000) /*TSTP/TEQP/CMPP/CMNP*/
			sprintf(s, "%sP%s R%i, #%x",
				dp_instructions[(opcode >> 21) & 0xf], cond,
				RN, rotatelookup[opcode & 0xfff]);
		else if ((opcode & 0x01800000) == 0x01000000) /*TST/TEQ/CMP/CMN*/
			sprintf(s, "%s%s R%i, #%x",
				dp_instructions[(opcode >> 21) & 0xf],
				cond, RN, rotatelookup[opcode & 0xfff]);
		else
			sprintf(s, "%s%s%s R%i, R%i, #%x",
				dp_instructions[(opcode >> 21) & 0xf], cond,
				(opcode & (1 << 20)) ? "S" : "",
				RD, RN, rotatelookup[opcode & 0xfff]);

		debug_out(s);
	}
	else if ((opcode & 0x0c000000) == 0x04000000) /*LDR/STR*/
	{
		switch (opcode & ((1 << 24) | (1 << 25)))
		{
			case 0: /*Immediate offset, post-indexing*/
			sprintf(s, "%s%s%s%s R%i, [R%i]",
				(opcode & (1 << 20)) ? "LDR" : "STR", cond,
				(opcode & (1 << 22)) ? "B" : "",
				(opcode & (1 << 21)) ? "T" : "",
				RD, RN);
			debug_out(s);
			if (opcode & 0xfff)
			{
				sprintf(s, ", #%s%x",
					(opcode & (1 << 23)) ? "" : "-",
					opcode & 0xfff);
				debug_out(s);
			}
			break;

			case (1 << 24): /*Immediate offset, pre-indexing*/
			sprintf(s, "%s%s%s R%i, [R%i",
				(opcode & (1 << 20)) ? "LDR" : "STR", cond,
				(opcode & (1 << 22)) ? "B" : "",
				RD, RN);
			debug_out(s);
			if (opcode & 0xfff)
			{
				sprintf(s, ", #%s%x",
					(opcode & (1 << 23)) ? "" : "-",
					opcode & 0xfff);
				debug_out(s);
			}
			debug_out("]");
			if (opcode & (1 << 21))
				debug_out("!");
			break;

			case (1 << 25): /*Register offset, post-indexing*/
			sprintf(s, "%s%s%s%s R%i, [R%i]",
				(opcode & (1 << 20)) ? "LDR" : "STR", cond,
				(opcode & (1 << 22)) ? "B" : "",
				(opcode & (1 << 21)) ? "T" : "",
				RD, RN);
			debug_out(s);
			if (opcode & 0xfff)
			{
				sprintf(s, ", %sR%i",
					(opcode & (1 << 23)) ? "" : "-",
					RM);
				debug_out(s);
				debug_print_shift(opcode);
			}
			break;

			case (1 << 24) | (1 << 25): /*Register offset, pre-indexing*/
			sprintf(s, "%s%s%s R%i, [R%i",
				(opcode & (1 << 20)) ? "LDR" : "STR", cond,
				(opcode & (1 << 22)) ? "B" : "",
				RD, RN);
			debug_out(s);
			if (opcode & 0xfff)
			{
				sprintf(s, ", %sR%i",
					(opcode & (1 << 23)) ? "" : "-",
					RM);
				debug_out(s);
				debug_print_shift(opcode);
			}
			debug_out("]");
			if (opcode & (1 << 21))
				debug_out("!");
			break;
		}
	}
	else if ((opcode & 0x0e000000) == 0x08000000) /*LDM/STM*/
	{
		sprintf(s, "%s%s%c%c R%i%s, {",
			(opcode & (1 << 20)) ? "LDM" : "STM",
			cond,
			(opcode & (1 << 23)) ? 'I' : 'D',
			(opcode & (1 << 24)) ? 'B' : 'A',
			RN,
			(opcode & (1 << 21)) ? "!" : "");
		debug_out(s);

		debug_print_register_list(opcode);

		debug_out("}");
		if (opcode & (1 << 22))
			debug_out("^");
	}
	else if ((opcode & 0x0e000000) == 0x0a000000) /*B/BL*/
	{
		sprintf(s, "%s%s %07x", (opcode & 0x01000000) ? "BL" : "B", cond, (debug_disaddr + 8 + ((opcode & 0xffffff) << 2)) & 0x3fffffc);
		debug_out(s);
	}
	else if ((opcode & 0x0e000000) == 0x0c000000) /*LDC/STC*/
	{
		if (opcode & (1 << 24)) /*Pre-indexing*/
		{
			sprintf(s, "%s%s%s CP%i, CR%i, [R%i",
				(opcode & (1 << 20)) ? "LDC" : "STC", cond,
				(opcode & (1 << 22)) ? "L" : "",
				(opcode >> 8) & 0xf,
				RD, RN);
			debug_out(s);
			if (opcode & 0xff)
			{
				sprintf(s, ", #%s%x",
					(opcode & (1 << 23)) ? "" : "-",
					opcode & 0xff);
				debug_out(s);
			}
			debug_out("]");
			if (opcode & (1 << 21))
				debug_out("!");
		}
		else /*Post indexing*/
		{
			sprintf(s, "%s%s%s%s CP%i, CR%i, [R%i]",
				(opcode & (1 << 20)) ? "LDC" : "STC", cond,
				(opcode & (1 << 22)) ? "L" : "",
				(opcode & (1 << 21)) ? "T" : "",
				(opcode >> 8) & 0xf,
				RD, RN);
			debug_out(s);
			if (opcode & 0xff)
			{
				sprintf(s, ", #%s%x",
					(opcode & (1 << 23)) ? "" : "-",
					opcode & 0xff);
				debug_out(s);
			}
		}
	}
	else if ((opcode & 0x0f000000) == 0x0e000000) /*CDP/MCR/MRC*/
	{
		if (opcode & (1 << 4))
		{
			sprintf(s, "%s%s CP%i, %i, R%i, CR%i, CR%i, %i",
				(opcode & (1 << 20)) ? "MCR" : "MRC",
				cond,
				(opcode >> 8) & 0xf,
				(opcode >> 21) & 7,
				RD, RN, RM,
				(opcode >> 5) & 7);
			debug_out(s);
		}
		else
		{
			sprintf(s, "CDP%s CP%i, %i, CR%i, CR%i, CR%i, %i",
				cond,
				(opcode >> 8) & 0xf,
				(opcode >> 20) & 0xf,
				RD, RN, RM,
				(opcode >> 5) & 7);
			debug_out(s);
		}
	}
	else if ((opcode & 0x0f000000) == 0x0f000000) /*SWI*/
	{
		const char *swi_name = debugger_swi_lookup(opcode);

		if (swi_name)
		{
			sprintf(s, "SWI%s %s%s", cond, (opcode & 0x20000) ? "X" : "", swi_name);
			debug_out(s);
		}
		else
		{
			sprintf(s, "SWI%s %06x", cond, opcode & 0xffffff);
			debug_out(s);
		}
	}
	else
	{
		sprintf(s, "UND%s", cond);
		debug_out(s);
	}

	debug_disaddr += 4;
}

static const char *trap_names[] =
{
	"Prefetch abort",
	"Data abort",
	"Address exception",
	"Undefined instruction",
	"SWI"
};

void debug_trap(int trap, uint32_t opcode)
{
	if (debug_trap_enable & (1 << trap))
	{
		uint32_t pc = (PC - 8) & 0x3fffffc;
		char s[256];

		if (trap == DEBUG_TRAP_SWI)
		{
			const char *swi_name = debugger_swi_lookup(opcode);

			if (swi_name)
				sprintf(s, "%s %s at %07x\n", trap_names[trap], swi_name, pc);
			else
				sprintf(s, "%s %06x at %07x\n", trap_names[trap], opcode & 0xffffff, pc);
		}
		else
			sprintf(s, "%s at %07x\n", trap_names[trap], pc);

		debug_out(s);

		debug = 1;
	}
}

#define NFSET ((armregs[15]&0x80000000)?1:0)
#define ZFSET ((armregs[15]&0x40000000)?1:0)
#define CFSET ((armregs[15]&0x20000000)?1:0)
#define VFSET ((armregs[15]&0x10000000)?1:0)
#define IFSET ((armregs[15]&0x08000000)?1:0)
#define FFSET ((armregs[15]&0x04000000)?1:0)

static const char *cpu_modes[4] =
{
	"user",
	"FIQ",
	"IRQ",
	"supervisor"
};

void debugger_do()
{
	uint32_t pc = (PC - 8) & 0x3fffffc;
	int c, d, e;
	int params;
	uint8_t temp;
	char outs[65536];
	char ins[256];

	if (debugger_in_reset)
		return;

	for (c = 0; c < 8; c++)
	{
		if (breakpoints[c] == pc)
		{
			debug = 1;
			sprintf(outs, "    Break at %07X\n", (PC-8));
			debug_out(outs);
		}
	}
	if (!debug)
		return;
//        if (!opcode) printf("BRK at %04X\n",pc);
	if (debug_step_count)
	{
		debug_step_count--;
		if (debug_step_count)
			return;
	}
	indebug = 1;
	console_input_enable();
	debug_disaddr = pc;
	while (1)
	{
		char command[256];
		char param1[256], param2[256], param3[256];
		int ret;

		d = debug_disaddr;
		debug_disaddr = pc;
		debug_disassemble();
		debug_disaddr = d;
		debug_out("\n");

		ret = console_input_get(ins);
		if (ret == CONSOLE_INPUT_GET_ERROR_WINDOW_CLOSED) /*Debugger console has been closed*/
		{
			debug = 0;
			debugon = 0;
			indebug = 0;
			return;
		}
		if (ret == CONSOLE_INPUT_GET_ERROR_IN_RESET) /*UI thread is trying to reset emulation, get out of the way*/
		{
			indebug = 0;
			return;
		}

		d = strlen(ins);
		debug_out("> ");
		debug_out(ins);
		debug_out("\n");

		params = sscanf(ins, "%s %s %s %s", command, param1, param2, param3);
		if (params <= 0)
		{
			strcpy(command, debug_lastcommand);
			params = 0;
		}
		else
			params--;

		switch (command[0])
		{
			case 'c': case 'C':
			debug = 0;
			indebug = 0;
			console_input_disable();
			return;
			case 'm': case 'M':
			{
			int mem_byte = !strncasecmp(command, "mb", 2);
			if (params)
				sscanf(param1, "%X", (unsigned int *)&debug_memaddr);
			for (c = 0; c < 16; c++)
			{
				uint32_t data[4];

				data[0] = readmemf_debug(debug_memaddr);
				data[1] = readmemf_debug(debug_memaddr+4);
				data[2] = readmemf_debug(debug_memaddr+8);
				data[3] = readmemf_debug(debug_memaddr+12);

				if (mem_byte)
					sprintf(outs, "    %07X : %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  ",
						debug_memaddr,
						data[0] & 0xff, (data[0] >> 8) & 0xff, (data[0] >> 16) & 0xff, (data[0] >> 24) & 0xff,
						data[1] & 0xff, (data[1] >> 8) & 0xff, (data[1] >> 16) & 0xff, (data[1] >> 24) & 0xff,
						data[2] & 0xff, (data[2] >> 8) & 0xff, (data[2] >> 16) & 0xff, (data[2] >> 24) & 0xff,
						data[3] & 0xff, (data[3] >> 8) & 0xff, (data[3] >> 16) & 0xff, (data[3] >> 24) & 0xff);
				else
					sprintf(outs, "    %07X : %08x %08x %08x %08x  ",
						debug_memaddr, data[0], data[1], data[2], data[3]);
				debug_out(outs);

				for (d = 0; d < 16; d++)
				{
					temp = data[d >> 2] >> ((d & 3) * 8);
					if (temp < 32)
						sprintf(outs, ".");
					else
						sprintf(outs, "%c", temp);
					debug_out(outs);
				}
				debug_memaddr += 16;
				debug_out("\n");
			}
			}
			break;
			case 'd': case 'D':
			if (params)
				sscanf(param1, "%X", (unsigned int *)&debug_disaddr);
			for (c = 0; c < 12; c++)
			{
				debug_out("    ");
				debug_disassemble();
				debug_out("\n");
			}
			break;
			case 'r': case 'R':
			if (params)
			{
				if (!strncasecmp(param1, "ioc", 3))
				{
					ioc_debug_print(outs);
					debug_out(outs);
				}
				else if (!strncasecmp(param1, "memc_cam", 8))
				{
					memc_debug_print_cam();
				}
				else if (!strncasecmp(param1, "memc", 4))
				{
					memc_debug_print(outs);
					debug_out(outs);
				}
				else if (!strncasecmp(param1, "vidc", 4))
				{
					vidc_debug_print(outs);
					debug_out(outs);
				}
			}
			else
			{
				sprintf(outs, "    ARM registers :\n");
				debug_out(outs);
				sprintf(outs, "    R0 =%08x R1 =%08x R2 =%08x R3 =%08x\n", armregs[0], armregs[1], armregs[2], armregs[3]);
				debug_out(outs);
				sprintf(outs, "    R4 =%08x R5 =%08x R6 =%08x R7 =%08x\n", armregs[4], armregs[5], armregs[6], armregs[7]);
				debug_out(outs);
				sprintf(outs, "    R8 =%08x R9 =%08x R10=%08x R11=%08x\n", armregs[8], armregs[9], armregs[10], armregs[11]);
				debug_out(outs);
				sprintf(outs, "    R12=%08x R13=%08x R14=%08x R15=%08x (PC=%07x)\n", armregs[12], armregs[13], armregs[14], armregs[15], PC-8);
				debug_out(outs);
				sprintf(outs, "    Status : %c%c%c%c%c%c     In %s mode\n",
					NFSET ? 'N' : ' ', ZFSET ? 'Z' : ' ',
					CFSET ? 'C' : ' ', VFSET ? 'V' : ' ',
					IFSET ? 'I' : ' ', FFSET ? 'F' : ' ',
					cpu_modes[armregs[15] & 3]);
				debug_out(outs);
			}
			break;
			case 's': case 'S':
			if (!strncasecmp(command, "save", 4))
			{
				if (params == 3)
				{
					uint32_t addr, size;

					sscanf(param2, "%X", (unsigned int *)&addr);
					sscanf(param3, "%X", (unsigned int *)&size);

					if (addr >= 0x4000000 || size == 0 || size >= 0x4000000 || (addr + size) > 0x4000000)
					{
						debug_out("Address or size out of range\n");
						break;
					}

					FILE *f = fopen(param1, "wb");
					for (int i = 0; i < size; i++)
					{
						uint32_t data = readmemf_debug(addr & ~3);
						putc(data >> (addr & 3) * 8, f);
						addr++;
					}
					fclose(f);
				}
				else
					debug_out("Syntax: save <fn> <addr> <size>\n");
			}
			else
			{
				if (params)
					sscanf(param1, "%i", &debug_step_count);
				else
					debug_step_count = 1;
				strcpy(debug_lastcommand, command);
				indebug = 0;
				console_input_disable();
				return;
			}
			break;
			case 'b': case 'B':
			if (!strncasecmp(command, "break", 5))
			{
				if (!params)
					break;
				for (c = 0; c < 8; c++)
				{
					if (breakpoints[c] == -1)
					{
						sscanf(param1, "%X", &breakpoints[c]);
						sprintf(outs, "    Breakpoint %i set to %04X\n", c, breakpoints[c]);
						debug_out(outs);
						break;
					}
				}
			}
			if (!strncasecmp(command, "blist", 5))
			{
				for (c = 0; c < 8; c++)
				{
					if (breakpoints[c] != -1)
					{
						sprintf(outs, "    Breakpoint %i : %04X\n", c, breakpoints[c]);
						debug_out(outs);
					}
				}
			}
			if (!strncasecmp(command, "bclear", 6))
			{
				if (!params)
					break;
				sscanf(param1, "%X", &e);
				for (c = 0; c < 8; c++)
				{
					if (breakpoints[c] == e)
						breakpoints[c] = -1;
					if (c == e)
						breakpoints[c] = -1;
				}
			}
			break;
			case 't': case 'T':
			if (!params)
			{
				sprintf(outs, "Trap status :\n"
					      "  Prefetch abort: %s\n"
					      "  Data abort: %s\n"
					      "  Address exception: %s\n"
					      "  Undefined instruction: %s\n"
					      "  SWI: %s\n\n",
					      (debug_trap_enable & (1 << DEBUG_TRAP_PREF_ABORT)) ? "enabled" : "disabled",
					      (debug_trap_enable & (1 << DEBUG_TRAP_DATA_ABORT)) ? "enabled" : "disabled",
					      (debug_trap_enable & (1 << DEBUG_TRAP_ADDR_EXCEP)) ? "enabled" : "disabled",
					      (debug_trap_enable & (1 << DEBUG_TRAP_UNDEF)) ? "enabled" : "disabled",
					      (debug_trap_enable & (1 << DEBUG_TRAP_SWI)) ? "enabled" : "disabled");
				debug_out(outs);
			}
			else if (params == 2)
			{
				int enable = !!strncasecmp(param1, "disable", 7);

				if (!strncasecmp(param2, "prefabort", 9))
				{
					if (enable)
						debug_trap_enable |= (1 << DEBUG_TRAP_PREF_ABORT);
					else
						debug_trap_enable &= ~(1 << DEBUG_TRAP_PREF_ABORT);
				}
				else if (!strncasecmp(param2, "dataabort", 9))
				{
					if (enable)
						debug_trap_enable |= (1 << DEBUG_TRAP_DATA_ABORT);
					else
						debug_trap_enable &= ~(1 << DEBUG_TRAP_DATA_ABORT);
				}
				else if (!strncasecmp(param2, "addrexcep", 9))
				{
					if (enable)
						debug_trap_enable |= (1 << DEBUG_TRAP_ADDR_EXCEP);
					else
						debug_trap_enable &= ~(1 << DEBUG_TRAP_ADDR_EXCEP);
				}
				else if (!strncasecmp(param2, "undefins", 8))
				{
					if (enable)
						debug_trap_enable |= (1 << DEBUG_TRAP_UNDEF);
					else
						debug_trap_enable &= ~(1 << DEBUG_TRAP_UNDEF);
				}
				else if (!strncasecmp(param2, "swi", 3))
				{
					if (enable)
						debug_trap_enable |= (1 << DEBUG_TRAP_SWI);
					else
						debug_trap_enable &= ~(1 << DEBUG_TRAP_SWI);
				}
			}
			break;
			case 'w': case 'W':
			if (!strncasecmp(command, "write", 5))
			{
				if (params != 2)
				{
					debug_out("Syntax: write[b] <addr> <data>\n");
					break;
				}

				uint32_t addr, data;

				sscanf(param1, "%X", (unsigned int *)&addr);
				sscanf(param2, "%X", (unsigned int *)&data);

				if (!strncasecmp(command, "writeb", 6))
					writememfb_debug(addr, data);
				else
					writememfl_debug(addr, data);
			}
			break;
			case 'h': case 'H': case '?':
			debug_out("\n    Debugger commands :\n\n");
			debug_out("    bclear <n>/<addr>       - clear breakpoint n or breakpoint at addr\n");
			debug_out("    blist                   - list current breakpoints\n");
			debug_out("    break <addr>            - set a breakpoint at addr\n");
			debug_out("    c                       - continue running indefinitely\n");
			debug_out("    d [addr]                - disassemble from address addr\n");
			debug_out("    m [addr]                - memory dump from address addr, in words\n");
			debug_out("    mb [addr]               - memory dump from address addr, in bytes\n");
			debug_out("    r                       - print ARM registers\n");
			debug_out("    r ioc                   - print IOC registers\n");
			debug_out("    r memc                  - print MEMC registers\n");
			debug_out("    r memc_cam              - print MEMC CAM mappings\n");
			debug_out("    r vidc                  - print VIDC registers\n");
			debug_out("    s [n]                   - step n instructions (or 1 if no parameter)\n");
			debug_out("    save <fn> <addr> <size> - save memory area to disc\n");
			debug_out("    t disable <type>        - disable trap\n");
			debug_out("    t enable <type>         - enable trap\n");
			debug_out("                              Available traps are prefabort, dataabort, addrexcep,\n");
			debug_out("                              undefins and swi\n");
			debug_out("    write <addr> <data>     - Write word to memory\n");
			debug_out("    writeb <addr> <data>    - Write byte to memory\n\n");
			break;
		}
		strcpy(debug_lastcommand, command);
	}
	console_input_disable();
	indebug = 0;
}

void debugger_start_reset(void)
{
	debugger_in_reset = 1;
}
void debugger_end_reset(void)
{
	debugger_in_reset = 0;
}
