/*Arculator 2.1 by Sarah Walker*/

/*State Machine G8/G16 Graphic Accelerator

  IOC Address map :
  0000-3fff - ROM

  MEMC Address map (only low 2 bits decoded) :
  0 - high byte latch for G332
  4 - G332 write, register address in A13-A4
  8 - (read) status  (write) G332 write mirror (used only for writing BOOT register?)
    bit 0 - IRQ status, cleared by read from 0x18?
  c - ROM bank

  VIDC data capture - uses supremacy bit to mark display area. Data is inverted
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arc.h"
#include "arm.h"
#include "g16.h"
#include "config.h"
#include "g332.h"
#include "podules.h"
#include "podule_api.h"

static const podule_callbacks_t *podule_callbacks;

typedef struct g16_t
{
	uint8_t rom[0x10000];
	uint8_t ram[0x80000];

	uint32_t wp;
	int vsync_state;

	unsigned int rom_bank;

	int irq_enable;

	uint16_t high_byte;
	uint8_t irq_status;
	uint8_t control;
	uint8_t fpga_control;

	g332_t g332;
	int64_t g332_time;

	podule_t *podule;
} g16_t;

static void g16_data_callback(uint8_t *data, int pixels, int hsync_length, int resolution, void *p);
static void g16_vsync_callback(void *p, int state);
static void g16_irq_callback(void *p, int state);

static int g16_init(struct podule_t *podule)
{
	FILE *f;
	char fn[512];
	const char *monitor_connection;

	g16_t *g16 = malloc(sizeof(g16_t));
	memset(g16, 0, sizeof(g16_t));

	append_filename(fn, exname, "roms/podules/g16/g16.rom", 511);
	f = fopen(fn, "rb");
	if (f)
	{
		fread(g16->rom, 0x10000, 1, f);
		fclose(f);
	}
	else
	{
		rpclog("g16_init failed\n");
		free(g16);
		return -1;
	}

	podule->p = g16;
	g16->podule = podule;

	g332_init(&g16->g332, g16->ram, INMOS_G332, g16_irq_callback, g16);
	vidc_attach(g16_data_callback, g16_vsync_callback, g16);

	monitor_connection = podule_callbacks->config_get_string(podule, "monitor_connection", "arc");
	if (!strcmp(monitor_connection, "podule"))
	{
		vidc_output_enable(0);
		g332_output_enable(&g16->g332, 1);
	}

	return 0;
}

static void g16_close(struct podule_t *podule)
{
	g16_t *g16 = podule->p;

	g332_close(&g16->g332);

	free(g16);
}


static void g16_data_callback(uint8_t *data, int pixels, int hsync_length, int resolution, void *p)
{
	g16_t *g16 = (g16_t *)p;
	int x;

	if (!g16->vsync_state)
	{
		int wp_nibble = 0;
		int sup_pixels = 0;

		for (x = 0; x < (pixels-1); x++)
		{
			if (data[x] & 0x10)
			{
				if (!wp_nibble)
					g16->ram[g16->wp & 0x7ffff] = (data[x] & 0xf) ^ 0xf;
				else
					g16->ram[g16->wp & 0x7ffff] |= ((data[x] & 0xf) ^ 0xf) << 4;
				sup_pixels++;

				wp_nibble = !wp_nibble;
				if (!wp_nibble)
					g16->wp++;
			}
		}
	}
}

static void g16_vsync_callback(void *p, int state)
{
	g16_t *g16 = (g16_t *)p;

	g16->vsync_state = state;

	if (state)
		g16->wp = 0;
}

static void g16_irq_callback(void *p, int state)
{
	g16_t *g16 = (g16_t *)p;

	if (state && !(g16->irq_status & 1))
	{
		g16->irq_status |= 1;
		if (g16->irq_enable)
			podule_callbacks->set_irq(g16->podule, 1);
	}
}


static uint8_t g16_read_b(struct podule_t *podule, podule_io_type type, uint32_t addr)
{
	g16_t *g16 = podule->p;
	uint8_t temp = 0xff;
	static int ff = 0;

	if (type != PODULE_IO_TYPE_IOC)
	{
		if ((addr & 0xc) == 8)
		{
			temp = 0x7f;
			ff = !ff;
			if (ff)
				temp ^= 0x40;
			if (addr & 0x10)
			{
				podule_callbacks->set_irq(g16->podule, 0);
				g16->irq_status &= ~1;
				g16->irq_enable = 1;
			}
		}
//                if ((addr & 0x3fff) != 8) rpclog("Read g16 MEMC %07X %02x %07X  %08x\n", addr, temp, PC, armregs[2]);
	}
	else
	{
		temp = g16->rom[((addr >> 2) & 0xfff) + ((g16->rom_bank << 12) & 0xf000)];
	}

	return temp;
}

static uint16_t g16_read_w(struct podule_t *podule, podule_io_type type, uint32_t addr)
{
	return g16_read_b(podule, type, addr);
}

static void g16_write_b(struct podule_t *podule, podule_io_type type, uint32_t addr, uint8_t val)
{
	g16_t *g16 = podule->p;

	if (type == PODULE_IO_TYPE_MEMC)
	{
//                if ((addr & 0x3fff) != 0xc) rpclog("Write g16 MEMC %07X %02X %07X\n",addr,val,PC);
		if ((addr & 0xc) == 0xc)
			g16->rom_bank = val;
	}
}

static void g16_write_w(struct podule_t *podule, podule_io_type type, uint32_t addr, uint16_t val)
{
	g16_t *g16 = podule->p;

	if (type == PODULE_IO_TYPE_MEMC)
	{
//                if ((addr & 0x3fff) != 0x0 && (addr & 0x3fff) != 0xc && (addr & 0xc) != 0x4) rpclog("Writew g16 MEMC %07X %04X %04x %07X\n",addr,val,g16->high_byte,PC);

		switch (addr & 0xc)
		{
			case 0x0:
			g16->high_byte = val;
			break;
			case 0x4: case 0x8:
			g332_write(&g16->g332, (addr >> 4) & 0x3ff, val | (g16->high_byte << 16));
			break;
			case 0xc:
			g16->rom_bank = val;
			break;
		}
	}
}

static int g16_run(struct podule_t *podule, int timeslice_us)
{
	g16_t *g16 = podule->p;

	g16->g332_time -= (uint64_t)timeslice_us << 32;

	while (g16->g332_time < 0)
		g16->g332_time += g332_poll(&g16->g332);

	return (g16->g332_time >> 32) + 1;
}

static podule_config_selection_t monitor_selection[] =
{
	{
		.description = "Archimedes",
		.value_string = "arc"
	},
	{
		.description = "Podule",
		.value_string = "podule"
	},
	{
		.description = ""
	}
};
static podule_config_t g16_config =
{
	.items =
	{
		{
			.name = "monitor_connection",
			.description = "Monitor connected to",
			.type = CONFIG_SELECTION_STRING,
			.selection = monitor_selection,
			.default_string = "arc"
		},
		{
			.type = -1
		}
	}
};

static const podule_header_t g16_podule_header =
{
	.version = PODULE_API_VERSION,
	.flags = PODULE_FLAGS_UNIQUE,
	.short_name = "g16",
	.name = "State Machine G16 Graphic Accelerator",
	.functions =
	{
		.init = g16_init,
		.close = g16_close,
		.read_b = g16_read_b,
		.read_w = g16_read_w,
		.write_b = g16_write_b,
		.write_w = g16_write_w,
		.run = g16_run
	},
	.config = &g16_config
};

const podule_header_t *g16_probe(const podule_callbacks_t *callbacks, char *path)
{
	FILE *f;
	char fn[512];

	podule_callbacks = callbacks;

	append_filename(fn, exname, "roms/podules/g16/g16.rom", 511);
	f = fopen(fn, "rb");
	if (!f)
		return NULL;
	fclose(f);
	return &g16_podule_header;
}
