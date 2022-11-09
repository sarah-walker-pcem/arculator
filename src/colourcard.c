/*Arculator 2.1 by Sarah Walker*/

/*Computer Concepts / Wild Vision Colourcard

  IOC Address map :
  0000-1fff : ROM (read)
  0000 - (write)
	bit 0 - FPGA serial data
	bit 1 - FPGA serial clock
  2000 - ROM bank register (write)
  2000 - IRQ status (read)
	bit 0 - IRQ (cleared on read?)
  3000 - Control register
	bit 2 - !VIDC passthrough (?)
	bit 7 - IRQ enable (?)
	others - unknown
	Set to 41 when VIDC driving display, cc when G332 driving

  MEMC Address map :
  0000-0fff : Inmos G332/5 register write
  2000 - High byte latch (write before writing to 0000-0fff

  VIDC data capture - starts 32 clocks (at 24 MHz) after hsync ends, terminates
    at start of next hsync
  only captures when !vsync
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arc.h"
#include "arm.h"
#include "colourcard.h"
#include "config.h"
#include "g332.h"
#include "podules.h"
#include "podule_api.h"

static const podule_callbacks_t *podule_callbacks;

typedef struct colourcard_t
{
	uint8_t rom[0x20000];
	uint8_t ram[0x80000];

	uint32_t wp;
	int vsync_state;

	unsigned int rom_bank;

	uint8_t high_byte;
	uint8_t irq_status;
	uint8_t control;
	uint8_t fpga_control;

	g332_t g332;
	int64_t g332_time;

	podule_t *podule;
} colourcard_t;

static void colourcard_data_callback(uint8_t *data, int pixels, int hsync_length, int resolution, void *p);
static void colourcard_vsync_callback(void *p, int state);
static void colourcard_irq_callback(void *p, int state);

static int colourcard_init(struct podule_t *podule)
{
	FILE *f;
	char fn[512];

	colourcard_t *colourcard = malloc(sizeof(colourcard_t));
	memset(colourcard, 0, sizeof(colourcard_t));

	append_filename(fn, exname, "roms/podules/colourcard/cc.bin", 511);
	f = fopen(fn, "rb");
	if (f)
	{
		fread(colourcard->rom, 0x20000, 1, f);
		fclose(f);
	}
	else
	{
		rpclog("colourcard_init failed\n");
		free(colourcard);
		return -1;
	}

	podule->p = colourcard;
	colourcard->podule = podule;

	g332_init(&colourcard->g332, colourcard->ram, INMOS_G335, colourcard_irq_callback, colourcard);
	vidc_attach(colourcard_data_callback, colourcard_vsync_callback, colourcard);

	return 0;
}

static void colourcard_close(struct podule_t *podule)
{
	colourcard_t *colourcard = podule->p;

	g332_close(&colourcard->g332);

	free(colourcard);
}

static void colourcard_data_callback(uint8_t *data, int pixels, int hsync_length, int resolution, void *p)
{
	colourcard_t *colourcard = (colourcard_t *)p;
	int x;

	if (!colourcard->vsync_state)
	{
		/*ColourCard horizontal offset is 32 clocks @ 24 MHz after hsync end*/
		int h_offset = hsync_length + (resolution ? 16 : 16*2) - 1;

		for (x = h_offset; x < (pixels-1); x += 2)
		{
			colourcard->ram[colourcard->wp & 0x7ffff] = data[x] | (data[x+1] << 4);
			colourcard->wp++;
		}
	}
}

static void colourcard_vsync_callback(void *p, int state)
{
	colourcard_t *colourcard = (colourcard_t *)p;

	colourcard->vsync_state = state;

	if (state)
		colourcard->wp = 0;
}

static void colourcard_irq_callback(void *p, int state)
{
	colourcard_t *colourcard = (colourcard_t *)p;

	if (state)
	{
		colourcard->irq_status |= 1;
		if (colourcard->control & 0x80)
			podule_callbacks->set_irq(colourcard->podule, 1);
	}
}

static uint8_t colourcard_read_b(struct podule_t *podule, podule_io_type type, uint32_t addr)
{
	colourcard_t *colourcard = podule->p;
	uint8_t temp = 0xff;

	if (type != PODULE_IO_TYPE_IOC)
	{
//                rpclog("Read Colourcard MEMC %07X %07X  %08x\n", addr, PC, armregs[2]);
		return 0xff; /*Only IOC accesses supported*/
	}
	switch (addr & 0x3000)
	{
		case 0x0000: case 0x1000:
		temp = colourcard->rom[((addr >> 2) & 0x7ff) + ((colourcard->rom_bank << 11) & 0x1f800)];
		break;

		case 0x2000:
		temp = colourcard->irq_status;
		colourcard->irq_status &= ~1;
		podule_callbacks->set_irq(colourcard->podule, 0);
		break;
	}

//        if (addr & 0x2000) rpclog("Read Colourcard %07X %02x %07X  %08x\n", addr, temp, PC, armregs[2]);
	return temp;
}

static uint16_t colourcard_read_w(struct podule_t *podule, podule_io_type type, uint32_t addr)
{
	uint16_t temp = 0xffff;

	if (type != PODULE_IO_TYPE_IOC)
	{
//                rpclog("Readw Colourcard MEMC %07X %07X  %08x\n", addr, PC, armregs[2]);
		return 0xffff; /*Only IOC accesses supported*/
	}
	switch (addr & 0x3000)
	{
		default:
		temp = colourcard_read_b(podule, type, addr);
		break;
	}

//        rpclog("Readw Colourcard %07X %02x %07X  %08x\n", addr, temp, PC, armregs[2]);
	return temp;
}

static void colourcard_write_b(struct podule_t *podule, podule_io_type type, uint32_t addr, uint8_t val)
{
	colourcard_t *colourcard = podule->p;

	if (type != PODULE_IO_TYPE_IOC)
	{
//                rpclog("Write Colourcard MEMC %07X %02X %07X\n",addr,val,PC);
		return; /*Only IOC accesses supported*/
	}

//        /*if ((addr & 0x3fff) != 0x2000) */rpclog("Write Colourcard %07X %02X %07X  %08x %08x\n",addr,val,PC, armregs[0], armregs[1]);
	switch (addr & 0x3000)
	{
		case 0x0000:
		colourcard->fpga_control = val;
		break;

		case 0x2000:
		colourcard->rom_bank = val;
		break;

		case 0x3000:
		colourcard->control = val;
//                rpclog("CC control=%02x\n", val);
		if ((colourcard->irq_status & 1) && (val & 0x80))
			podule_callbacks->set_irq(colourcard->podule, 1);
		else
			podule_callbacks->set_irq(colourcard->podule, 0);
		vidc_output_enable(!(val & 4));
		g332_output_enable(&colourcard->g332, val & 4);
		break;
	}
}

static void colourcard_write_w(struct podule_t *podule, podule_io_type type, uint32_t addr, uint16_t val)
{
	colourcard_t *colourcard = podule->p;

	if (type == PODULE_IO_TYPE_MEMC)
	{
//                rpclog("Writew Colourcard MEMC %07X %04X %07X\n",addr,val,PC);

		switch (addr & 0x3000)
		{
			case 0x0000:
//                        rpclog(" IMS write %03x %06x\n", (addr & 0xfff) >> 2, val | (colourcard->high_byte << 16));
			g332_write(&colourcard->g332, (addr & 0xfff) >> 2, val | (colourcard->high_byte << 16));
			break;

			case 0x2000:
			colourcard->high_byte = val;
			break;
		}
	}
	else
	{
//                rpclog("Writew Colourcard %07X %04X %07X\n",addr,val,PC);

		switch (addr & 0x3000)
		{
			default:
			colourcard_write_b(podule, type, addr, val & 0xff);
			break;
		}
	}
}

static int colourcard_run(struct podule_t *podule, int timeslice_us)
{
	colourcard_t *colourcard = podule->p;

	colourcard->g332_time -= (uint64_t)timeslice_us << 32;

	while (colourcard->g332_time < 0)
		colourcard->g332_time += g332_poll(&colourcard->g332);

	return (colourcard->g332_time >> 32) + 1;
}

static const podule_header_t colourcard_podule_header =
{
	.version = PODULE_API_VERSION,
	.flags = PODULE_FLAGS_UNIQUE,
	.short_name = "colourcard",
	.name = "Computer Concepts Colour Card",
	.functions =
	{
		.init = colourcard_init,
		.close = colourcard_close,
		.read_b = colourcard_read_b,
		.read_w = colourcard_read_w,
		.write_b = colourcard_write_b,
		.write_w = colourcard_write_w,
		.run = colourcard_run
	}
};

const podule_header_t *colourcard_probe(const podule_callbacks_t *callbacks, char *path)
{
	FILE *f;
	char fn[512];

	podule_callbacks = callbacks;

	append_filename(fn, exname, "roms/podules/colourcard/cc.bin", 511);
	f = fopen(fn, "rb");
	if (!f)
		return NULL;
	fclose(f);
	return &colourcard_podule_header;
}
