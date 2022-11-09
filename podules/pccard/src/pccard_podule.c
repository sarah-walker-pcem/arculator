/*Aleph One 386/486PC Expansion Card (Diva / "slow PAL")

  IOC address map :
  0000-3fff - ROM

  MEMC Address map (only A4/A5 decoded) :
  00 - Interrupt source status register
  	bits 0-3 - high 4 bits of mailbox address
  		= 1 for IO
  		>= A for memory
  	bit 4 - is read access
  	bit 5 - is word access
  	bit 7 - ISSR contents valid
  10 - Interrupt source control register
	bit 0 - POWERGOOD
	bit 1 - HISPEED
	bit 6 - raise IRQ1 (keyboard)
	bit 7 - raise IRQ8 (RTC)
	bit 8 - raise IRQ3 (COM2)
	bit 9 - raise IRQ6 (floppy)
	bit 10 - raise IRQ5 (LPT2)
	bit 11 - raise IRQ15 (HPC)
  20 - Mailbox data register
  	read clears pending write and resumes 386
  	write clears pending read and resumes 386
  30 - Mailbox address register


  386 side is a basic 386SX design, using a SCAMP to provide most motherboard functions (DRAM control,
  PIC, PIT, DMA etc), and a UM82C452 for the onboard serial and parallel ports. Non-shadowed memory
  accesses between 0A0000-0FFFFF, as well as all IO accesses, will trap with address + data becoming
  present in the ARM-side mailbox. This allows for emulation of the missing components. The ARM can
  also trigger interrupts on the 386 side.

  PAL-based cards allow for either 1 or 4 MB of RAM on board, with links determining the SCAMP RAMMAP
  register on reset.

  There is no ROM on the 386 side, accesses to the BIOS area are trapped and emulated. On initial boot
  the BIOS code will copy itself to shadow RAM and disable ROM at that point.

  CPUs were an Intel 386SX/25 on the 386PC card and a Cyrix 486SLC/25 on the 486PC card. BIOS source
  suggests a TI 486SXLC2/50 was also an option, I am unsure if any PAL-based cards were shipped with
  this CPU or if this was only used on the later Elvis cards.

  The register interface seems to be the same for both "slow PAL" and "fast PAL" cards. Only difference
  is that the ARM-side code for "fast PAL" performs fewer dummy register accesses. PROM is likely
  different so that the cards can be distinguished.
*/
/*Current emulation is derived from PCem V17. As PCem is not designed to break from the main loop
  in the middle of an instruction (required for memory/IO trapping), a cothread implementation is
  used provided by libco (https://github.com/higan-emu/libco).*/
#ifdef WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include "podule_api.h"
#include "pccard_podule.h"
#include "pcem/diva.h"

void dumpregs();

#ifdef WIN32
extern __declspec(dllexport) const podule_header_t *podule_probe(const podule_callbacks_t *callbacks, char *path);
#else
#define BOOL int
#define APIENTRY
#endif

const podule_callbacks_t *podule_callbacks;
char podule_path[512];

typedef struct pccard_t
{
	int temp;
} pccard_t;

static FILE *pccard_logf;
/*Taken from 386SX "slow PAL" Diva-I podule*/
static const uint8_t prom[256] =
{
	0x00, 0x03, 0x00, 0xff, 0x00, 0xff, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xf5, 0x21, 0x00, 0x00, 0x34, 0x00, 0x00, 0x00,
	0xf4, 0x24, 0x00, 0x00, 0x55, 0x00, 0x00, 0x00,  0xf2, 0x10, 0x00, 0x00, 0x79, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x41, 0x6c, 0x65, 0x70,  0x68, 0x20, 0x4f, 0x6e, 0x65, 0x20, 0x33, 0x38,
	0x36, 0x20, 0x50, 0x43, 0x20, 0x20, 0x20, 0x20,  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x00, 0x41, 0x6c, 0x65,  0x70, 0x68, 0x20, 0x4f, 0x6e, 0x65, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x2c, 0x20, 0x43, 0x61,  0x6d, 0x62, 0x72, 0x69, 0x64, 0x67, 0x65, 0x2c,
	0x20, 0x45, 0x6e, 0x67, 0x6c, 0x61, 0x6e, 0x64,  0x00, 0x54, 0x75, 0x65, 0x2c, 0x31, 0x30, 0x20,

	0x44, 0x65, 0x63, 0x20, 0x31, 0x39, 0x39, 0x31,  0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

void pclog(const char *format, ...)
{
   	char buf[1024];
   	va_list ap;

	return;

	if (!pccard_logf)
		pccard_logf = fopen("e:/devcpp/Arculator/pccard/pclog.txt", "wt");

   	va_start(ap, format);
   	vsprintf(buf, format, ap);
   	va_end(ap);
   	fputs(buf, pccard_logf);
   	fflush(pccard_logf);
}

void fatal(const char *format, ...)
{
   	char buf[1024];
   	va_list ap;

//	return;

	if (!pccard_logf)
		pccard_logf = fopen("pclog.txt", "wt");

   	va_start(ap, format);
   	vsprintf(buf, format, ap);
   	va_end(ap);
   	fputs("FATAL : ", pccard_logf);
   	fputs(buf, pccard_logf);
//   	dumpregs();
   	fflush(pccard_logf);
   	exit(-1);
}

static uint8_t pccard_ioc_readb(podule_t *podule, uint32_t addr)
{
	pccard_t *pccard = podule->p;

//        pclog("Read pccard B %04X\n", addr);

	return prom[(addr >> 2) & 0xff];
}

static uint8_t pccard_memc_readb(podule_t *podule, uint32_t addr)
{
	pccard_t *pccard = podule->p;

	//pclog("Read pccard MEMC B %04X\n", addr);

       	return diva_arm_read(addr);
}


static uint16_t pccard_memc_readw(podule_t *podule, uint32_t addr)
{
	pccard_t *pccard = podule->p;

	//pclog("Read pccard MEMC W %04X\n", addr);

	return diva_arm_read(addr);//pccard_memc_readb(podule, addr);
}


static void pccard_ioc_writeb(podule_t *podule, uint32_t addr, uint8_t val)
{
	pccard_t *pccard = podule->p;

//        pclog("Write pccard IOC B %04X %02X\n", addr, val);
}

static void pccard_memc_writeb(podule_t *podule, uint32_t addr, uint8_t val)
{
	pccard_t *pccard = podule->p;

	//pclog("Write pccard MEMC B %04X %02X\n", addr, val);

	diva_arm_write(addr, val);
}


static void pccard_memc_writew(podule_t *podule, uint32_t addr, uint16_t val)
{
	pccard_t *pccard = podule->p;

	//pclog("Write pccard MEMC W %04X %02X\n", addr, val);

	diva_arm_write(addr, val);
	//pccard_memc_writeb(podule, addr, val);
}


static uint8_t pccard_read_b(struct podule_t *podule, podule_io_type type, uint32_t addr)
{
	if (type == PODULE_IO_TYPE_IOC)
		return pccard_ioc_readb(podule, addr);
	else if (type == PODULE_IO_TYPE_MEMC)
		return pccard_memc_readb(podule, addr);

	return 0xff;
}

static uint16_t pccard_read_w(struct podule_t *podule, podule_io_type type, uint32_t addr)
{
	if (type == PODULE_IO_TYPE_IOC)
		return pccard_ioc_readb(podule, addr);
	else if (type == PODULE_IO_TYPE_MEMC)
		return pccard_memc_readw(podule, addr);

	return 0xffff;
}

static void pccard_write_b(struct podule_t *podule, podule_io_type type, uint32_t addr, uint8_t val)
{
	if (type == PODULE_IO_TYPE_IOC)
		pccard_ioc_writeb(podule, addr, val);
	else if (type == PODULE_IO_TYPE_MEMC)
		pccard_memc_writeb(podule, addr, val);
}

static void pccard_write_w(struct podule_t *podule, podule_io_type type, uint32_t addr, uint16_t val)
{
	if (type == PODULE_IO_TYPE_IOC)
		pccard_ioc_writeb(podule, addr, val);
	else if (type == PODULE_IO_TYPE_MEMC)
		pccard_memc_writew(podule, addr, val);
}



static void pccard_reset(struct podule_t *podule)
{
	pccard_t *pccard = podule->p;

	pclog("Reset pccard\n");
}

static int pccard_init(struct podule_t *podule)
{
	const char *cpu_type;
	int mem_size_mb;
	int pccard_cpu_type;
	int fpu_present;

	pccard_t *pccard = malloc(sizeof(pccard_t));
	memset(pccard, 0, sizeof(pccard_t));

	podule->p = pccard;

	cpu_type = podule_callbacks->config_get_string(podule, "cpu_type", "386sx");
	pclog("cpu_type=%s\n", cpu_type);
	if (!strcmp(cpu_type, "486sxlc2"))
		pccard_cpu_type = PCCARD_CPU_486SXLC2;
	else if (!strcmp(cpu_type, "486slc"))
		pccard_cpu_type = PCCARD_CPU_486SLC;
	else
		pccard_cpu_type = PCCARD_CPU_386SX;
	mem_size_mb = podule_callbacks->config_get_int(podule, "mem_size", 4);
	pclog("mem_size_mb=%i\n", mem_size_mb);
	fpu_present = podule_callbacks->config_get_int(podule, "fpu", 0);
	pclog("fpu_present=%i\n", fpu_present);

	pc_init(pccard_cpu_type, fpu_present, mem_size_mb);

	return 0;
}

static void pccard_close(struct podule_t *podule)
{
	pccard_t *pccard = podule->p;

	dumpregs();

	free(pccard);
}

static int pccard_run(struct podule_t *podule, int timeslice_us)
{
	pccard_t *pccard = podule->p;

	return diva_run(timeslice_us);
}

static podule_config_selection_t pccard_cpu_selection[] =
{
	{
		.description = "386SX/25",
		.value_string = "386sx"
	},
	{
		.description = "486SLC/25",
		.value_string = "486slc"
	},
	{
		.description = "486SXLC2/50",
		.value_string = "486sxlc2"
	},
	{
		.description = ""
	}
};
static podule_config_selection_t pccard_mem_selection[] =
{
	{
		.description = "1 MB",
		.value = 1
	},
	{
		.description = "4 MB",
		.value = 4
	},
	{
		.description = ""
	}
};
podule_config_t pccard_podule_config =
{
	.items =
	{
		{
			.name = "cpu_type",
			.description = "CPU",
			.type = CONFIG_SELECTION_STRING,
			.selection = pccard_cpu_selection,
			.default_string = "386sx",
		},
		{
			.name = "fpu",
			.description = "80387 present",
			.type = CONFIG_BINARY,
			.default_int = 0
		},
		{
			.name = "mem_size",
			.description = "Memory",
			.type = CONFIG_SELECTION,
			.selection = pccard_mem_selection,
			.default_int = 4,
		},
		{
			.type = -1
		}
	}
};

static const podule_header_t pccard_podule_header =
{
	.version = PODULE_API_VERSION,
	.flags = PODULE_FLAGS_UNIQUE,
	.short_name = "pccard",
	.name = "Aleph One PC Card",
	.functions =
	{
		.init = pccard_init,
		.close = pccard_close,
		.reset = pccard_reset,
		.read_b = pccard_read_b,
		.read_w = pccard_read_w,
		.write_b = pccard_write_b,
		.write_w = pccard_write_w,
		.run = pccard_run
	},
	.config = &pccard_podule_config
};

const podule_header_t *podule_probe(const podule_callbacks_t *callbacks, char *path)
{
	pclog("podule_probe %p path=%s\n", &pccard_podule_header, path);

	podule_callbacks = callbacks;
	strcpy(podule_path, path);

	return &pccard_podule_header;
}

#ifdef WIN32
BOOL APIENTRY DllMain (HINSTANCE hInst     /* Library instance handle. */ ,
		       DWORD reason        /* Reason this function is being called. */ ,
		       LPVOID reserved     /* Not used. */ )
{
//        pclog("DllMain %i\n", reason);
    switch (reason)
    {
      case DLL_PROCESS_ATTACH:
	break;

      case DLL_PROCESS_DETACH:
	break;

      case DLL_THREAD_ATTACH:
	break;

      case DLL_THREAD_DETACH:
	break;
    }

    /* Returns TRUE on success, FALSE on failure */
    return TRUE;
}
#endif
