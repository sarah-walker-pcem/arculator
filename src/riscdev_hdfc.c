/*Arculator 2.1 by Sarah Walker*/

/*RISC Developments High Density Floppy Controller

  IOC Address map :
  0000-1fff : ROM (read)
	      ROM page register (write)
  2000 : status (read)
	bit 2 - index IRQ
	bit 3 - FDC IRQ
	 DMA byte count low (write)
  3000 : clear index IRQ?  (read) (value discarded)
	 DMA byte count high (write)

  MEMC Address map :
  0000-001c : FDC
  2000-2fff : DMA (address 2ffc used)
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arc.h"
#include "82c711_fdc.h"
#include "config.h"
#include "podules.h"
#include "podule_api.h"

static const podule_callbacks_t *podule_callbacks;

typedef struct hdfc_t
{
	int page;
	uint8_t rom[0x10000];

	uint16_t dma_count;

	int irq_status;

	podule_t *podule;

	void *fdc;
} hdfc_t;

#define HDFC_INDEX_IRQ (1 << 2)
#define HDFC_FDC_IRQ   (1 << 3)

static void hdfc_fdc_irq(int state, void *p);
static void hdfc_fdc_index_irq(void *p);
static void hdfc_fdc_fiq(int state, void *p);

static int hdfc_init(struct podule_t *podule)
{
	FILE *f;
	char fn[512];

	hdfc_t *hdfc = malloc(sizeof(hdfc_t));
	memset(hdfc, 0, sizeof(hdfc_t));

	append_filename(fn, exname, "roms/podules/hdfc/hdfc.rom", 511);
	f = fopen(fn, "rb");
	if (f)
	{
		fread(hdfc->rom, 0x10000, 1, f);
		fclose(f);
	}
	else
	{
		rpclog("hdfc_init failed\n");
		free(hdfc);
		return -1;
	}

	podule->p = hdfc;
	hdfc->podule = podule;

	hdfc->fdc = c82c711_fdc_init_override(hdfc_fdc_irq, hdfc_fdc_index_irq, hdfc_fdc_fiq, hdfc);

	return 0;
}

static void hdfc_reset(struct podule_t *podule)
{
	hdfc_t *hdfc = podule->p;

	hdfc->page = 0;
}

static void hdfc_close(struct podule_t *podule)
{
	hdfc_t *hdfc = podule->p;

	free(hdfc);
}

static uint8_t hdfc_read_b(struct podule_t *podule, podule_io_type type, uint32_t addr)
{
	hdfc_t *hdfc = podule->p;
	uint8_t temp = 0xff;

	if (type != PODULE_IO_TYPE_IOC)
	{
		rpclog("hdfc_read_b: MEMC %08x\n", addr);
		switch (addr & 0x3000)
		{
			case 0x0000:
			return c82c711_fdc_read((addr >> 2) & 7, hdfc->fdc);

			case 0x2000:
			hdfc->dma_count--;
			return c82c711_fdc_dmaread((hdfc->dma_count == 0), hdfc->fdc);
		}
		return 0xff; /*Only IOC accesses supported*/
	}

//        if (addr & 0x2000)
//                rpclog("hdfc_read_b: IOC %08x\n", addr);
	if (addr & 0x2000)
		rpclog("hdfc_read_b: IOC %08x %07x  r2=%02x\n", addr, PC, armregs[2]);
	switch (addr & 0x3800)
	{
		case 0x0000: case 0x0800: case 0x1000: case 0x1800:
		addr = ((addr & 0x1ffc) | (hdfc->page << 13)) >> 2;
//                rpclog("  hdfc_read_b: IOC ROM %08x\n", addr);
		temp = hdfc->rom[addr & 0xffff];
		break;

		case 0x2000:
		temp = hdfc->irq_status;
		break;

		case 0x3000:
		hdfc->irq_status &= ~HDFC_INDEX_IRQ;
		podule_callbacks->set_irq(podule, hdfc->irq_status);
		break;

		default:
		break;
	}

	return temp;
}

static uint16_t hdfc_read_w(struct podule_t *podule, podule_io_type type, uint32_t addr)
{
	if (type != PODULE_IO_TYPE_IOC)
	{
		rpclog("hdfc_read_w: MEMC %08x\n", addr);
		return 0xffff; /*Only IOC accesses supported*/
	}

//        if (addr & 0x2000)
		rpclog("hdfc_read_w: IOC %08x\n", addr);
	switch (addr & 0x3800)
	{
		default:
		return hdfc_read_b(podule, type, addr);
	}
}

static void hdfc_write_b(struct podule_t *podule, podule_io_type type, uint32_t addr, uint8_t val)
{
	hdfc_t *hdfc = podule->p;

	if (type != PODULE_IO_TYPE_IOC)
	{
		rpclog("hdfc_write_b: MEMC %08x %02x\n", addr, val);
		switch (addr & 0x3000)
		{
			case 0x0000:
			c82c711_fdc_write((addr >> 2) & 7, val, hdfc->fdc);
			break;

			case 0x2000:
			hdfc->dma_count--;
			c82c711_fdc_dmawrite(val, (hdfc->dma_count == 0), hdfc->fdc);
			break;
		}
		return; /*Only IOC accesses supported*/
	}

	//rpclog("Write ICS %07X %02X %07X\n",addr,val,PC);
	if (addr & 0x2000)
		rpclog("hdfc_write_b: IOC %08x %02x %07x\n", addr, val, PC);
	switch (addr & 0x3800)
	{
		case 0x0000:
		hdfc->page = val;
		break;

		case 0x2000:
		hdfc->dma_count = (hdfc->dma_count & 0xff00) | val;
		break;

		case 0x3000:
		hdfc->dma_count = (hdfc->dma_count & 0x00ff) | (val << 8);
		break;

		default:
//                rpclog("hdfc_write_b: IOC %08x %02x\n", addr, val);
		break;
	}
}

static void hdfc_write_w(struct podule_t *podule, podule_io_type type, uint32_t addr, uint16_t val)
{
	if (type != PODULE_IO_TYPE_IOC)
	{
		rpclog("hdfc_write_w: MEMC %08x %04x\n", addr, val);
		return; /*Only IOC accesses supported*/
	}

	rpclog("hdfc_write_w: IOC %08x %04x\n", addr, val);
	switch (addr & 0x3800)
	{
		default:
		hdfc_write_b(podule, type, addr, val & 0xff);
		break;
	}
}

static void hdfc_fdc_irq(int state, void *p)
{
	hdfc_t *hdfc = (hdfc_t *)p;

	if (state)
		hdfc->irq_status |= HDFC_FDC_IRQ;
	else
		hdfc->irq_status &= ~HDFC_FDC_IRQ;

	podule_callbacks->set_irq(hdfc->podule, hdfc->irq_status);
}

static void hdfc_fdc_index_irq(void *p)
{
	hdfc_t *hdfc = (hdfc_t *)p;

	hdfc->irq_status |= HDFC_INDEX_IRQ;
	podule_callbacks->set_irq(hdfc->podule, hdfc->irq_status);
}

static void hdfc_fdc_fiq(int state, void *p)
{
	hdfc_t *hdfc = (hdfc_t *)p;

	podule_callbacks->set_fiq(hdfc->podule, state);
}

static const podule_header_t hdfc_podule_header =
{
	.version = PODULE_API_VERSION,
	.flags = PODULE_FLAGS_UNIQUE,
	.short_name = "hdfc",
	.name = "RISC Developments High Density Floppy Controller",
	.functions =
	{
		.init = hdfc_init,
		.close = hdfc_close,
		.reset = hdfc_reset,
		.read_b = hdfc_read_b,
		.read_w = hdfc_read_w,
		.write_b = hdfc_write_b,
		.write_w = hdfc_write_w
	}
};

const podule_header_t *riscdev_hdfc_probe(const podule_callbacks_t *callbacks, char *path)
{
	FILE *f;
	char fn[512];

	podule_callbacks = callbacks;

	append_filename(fn, exname, "roms/podules/hdfc/hdfc.rom", 511);
	f = fopen(fn, "rb");
	if (!f)
		return NULL;
	fclose(f);
	return &hdfc_podule_header;
}
