/*Arculator 2.1 by Sarah Walker*/

/*ICS A3000 IDE Interface v5

  IOC Address map :
  0000-1fff : ROM (read)
	      ROM bank (write)

  MEMC Address map :
  2000-21ff, A2 low : IDE registers - bits 6-8 = register index
	     A2 high : high byte latch
  2380 : IDE alternate status
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arc.h"
#include "config.h"
#include "ide_a3in.h"
#include "ide.h"
#include "podules.h"
#include "podule_api.h"
#include "ide_config.h"

static const podule_callbacks_t *podule_callbacks;

typedef struct ide_a3in_ide_t
{
	int page;
	uint8_t rom[32768];

	uint8_t high_byte_latch;

	ide_t ide;
} ide_a3in_ide_t;

static void ide_a3in_ide_irq_raise();
static void ide_a3in_ide_irq_clear();

static int ics_a3in_ide_init(struct podule_t *podule)
{
	FILE *f;
	char fn[512];
	char hd4_fn[512] = {0}, hd5_fn[512] = {0};
	int hd_spt[2], hd_hpc[2], hd_cyl[2];
	const char *p;

	ide_a3in_ide_t *a3in = malloc(sizeof(ide_a3in_ide_t));
	memset(a3in, 0, sizeof(ide_a3in_ide_t));

	append_filename(fn, exname, "roms/podules/a3inv5/ICS 93 A3IN 3V5 3V06 - 256.BIN", 511);
	f = fopen(fn, "rb");
	if (f)
	{
		fread(a3in->rom, 0x8000, 1, f);
		fclose(f);
	}
	else
	{
		rpclog("a3in_ide_init failed\n");
		free(a3in);
		return -1;
	}

	p = podule_callbacks->config_get_string(podule, "hd4_fn", "");
	if (p)
		strcpy(hd4_fn, p);
	hd_spt[0] = podule_callbacks->config_get_int(podule, "hd4_sectors", 63);
	hd_hpc[0] = podule_callbacks->config_get_int(podule, "hd4_heads", 16);
	hd_cyl[0] = podule_callbacks->config_get_int(podule, "hd4_cylinders", 100);
	p = podule_callbacks->config_get_string(podule, "hd5_fn", "");
	if (p)
		strcpy(hd5_fn, p);
	hd_spt[1] = podule_callbacks->config_get_int(podule, "hd4_sectors", 63);
	hd_hpc[1] = podule_callbacks->config_get_int(podule, "hd4_heads", 16);
	hd_cyl[1] = podule_callbacks->config_get_int(podule, "hd4_cylinders", 100);

	resetide(&a3in->ide,
		 hd4_fn, hd_spt[0], hd_hpc[0], hd_cyl[0],
		 hd5_fn, hd_spt[1], hd_hpc[1], hd_cyl[1],
		 ide_a3in_ide_irq_raise, ide_a3in_ide_irq_clear);

	podule->p = a3in;

	return 0;
}

static void ide_a3in_ide_reset(struct podule_t *podule)
{
	ide_a3in_ide_t *a3in = podule->p;

	a3in->page = 0;
}

static void ide_a3in_ide_close(struct podule_t *podule)
{
	ide_a3in_ide_t *a3in = podule->p;

	closeide(&a3in->ide);
	free(a3in);
}

static void ide_a3in_ide_irq_raise(ide_t *ide)
{
}

static void ide_a3in_ide_irq_clear(ide_t *ide)
{
}

static uint8_t ide_a3in_ide_read_b(struct podule_t *podule, podule_io_type type, uint32_t addr)
{
	ide_a3in_ide_t *a3in = podule->p;
	uint8_t temp = 0xff;

	if (type != PODULE_IO_TYPE_IOC)
	{
		switch (addr & 0x3e00)
		{
			case 0x2000:
			if (addr & 4)
				temp = a3in->high_byte_latch;
			else if (!(addr & 0x1c0))
			{
				uint16_t tempw = readidew(&a3in->ide);

				a3in->high_byte_latch = tempw >> 8;
				temp = tempw & 0xff;
			}
			else
				temp = readide(&a3in->ide, ((addr >> 6) & 7) + 0x1f0);
			break;
			case 0x2200:
			if ((addr & 0x1c0) == 0x180)
				temp = readide(&a3in->ide, 0x3f6);
			break;
		}
//                rpclog("Read MEMC a3in %07X %02x %07X\n", addr, temp, PC);
		return temp;
	}
	else
	{
		switch (addr & 0x3c00)
		{
			case 0x0000: case 0x0400: case 0x0800: case 0x0c00:
			case 0x1000: case 0x1400: case 0x1800: case 0x1c00:
			{
				uint32_t rom_addr = ((addr & 0x1ffc) | (a3in->page << 13)) >> 2;
//                                rpclog(" Read IROM %04X %i %04x %02X\n",addr,a3in->page,rom_addr,a3in->rom[rom_addr & 0x7fff]);
				temp = a3in->rom[rom_addr & 0x7fff];
			}
			break;
		}

//                rpclog("Read a3in %07X %02x %07X\n", addr, temp, PC);
	}
	return temp;
}

static void ide_a3in_ide_write_b(struct podule_t *podule, podule_io_type type, uint32_t addr, uint8_t val)
{
	ide_a3in_ide_t *a3in = podule->p;

	if (type != PODULE_IO_TYPE_IOC)
	{
		switch (addr & 0x3e00)
		{
			case 0x2000:
			if (addr & 4)
				a3in->high_byte_latch = val;
			else if (!(addr & 0x1c0))
				writeidew(&a3in->ide, val | (a3in->high_byte_latch << 8));
			else
				writeide(&a3in->ide, ((addr >> 6) & 7) + 0x1f0, val);
			break;
		}
//                rpclog("Write MEMC a3in %07X %02X %07X\n",addr,val,PC);
	}
	else
	{
//                rpclog("Write a3in %07X %02X %07X\n",addr,val,PC);
		switch (addr & 0x3c00)
		{
			case 0x0000:
			if (!addr)
				a3in->page = val;
			break;
		}
	}
}

static const podule_header_t ics_a3inv5_ide_podule_header =
{
	.version = PODULE_API_VERSION,
	.flags = PODULE_FLAGS_UNIQUE | PODULE_FLAGS_8BIT,
	.short_name = "a3inv5",
	.name = "ICS A3000 IDE Interface v5",
	.functions =
	{
		.init = ics_a3in_ide_init,
		.close = ide_a3in_ide_close,
		.reset = ide_a3in_ide_reset,
		.read_b = ide_a3in_ide_read_b,
		.write_b = ide_a3in_ide_write_b,
	},
	.config = &ide_podule_config
};

const podule_header_t *ics_a3inv5_ide_probe(const podule_callbacks_t *callbacks, char *path)
{
	FILE *f;
	char fn[512];

	podule_callbacks = callbacks;
	ide_config_init(callbacks);

	append_filename(fn, exname, "roms/podules/a3inv5/ICS 93 A3IN 3V5 3V06 - 256.BIN", 511);
	f = fopen(fn, "rb");
	if (!f)
		return NULL;
	fclose(f);
	return &ics_a3inv5_ide_podule_header;
}
