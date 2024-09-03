/*HCCS 8-bit IDE podule

  Basic IDE card in mini-podule format, with a user port (not implemented in
  this emulation). This card does not support IDE interrupts.

  https://chrisacorns.computinghistory.org.uk/32bit_UpgradesH2Z/HCCS_IDE_A3000.html

  IOC Address map :
	0000-1fff : ROM
	2000-203f : VIA (ROM paging register on port A)
	2100-213f : IDE registers
	2140-217f : IDE alternate registers
	2200 	  : High byte latch (write)
	2300 	  : High byte latch (read)

  Address map and semantics have been determined by reverse engineering and may
  not be 100% correct. However, it's correct enough that HCCS IDEFS works
  properly. Only Port A and DDR A of the VIA are emulated, for purposes of the
  paging register. User port, timers, and unused Port A functionality are
  unimplemented - softwate that uses these will probably not work.
  
  Note that when configuring IDEFS, '*Configure IDEFSDelay 8' is required, per
  the HCCS IDE manual; this setting defaults to 0, which will NOT work.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arc.h"
#include "config.h"
#include "ide.h"
#include "podules.h"
#include "podule_api.h"
#include "ide_config.h"

static const podule_callbacks_t *podule_callbacks;

/* Cards appear to all come with a 27C256 EPROM installed, but have provision
for a 27C512 or 27C010, so let's leave enough room for a 128K image */
#define HCCS_A3K_IDE_ROM_MAX_SIZE 131072

#define VIA_PORTA 1
#define VIA_DDRA 3

typedef struct hccs_a3k_ide_t
{
	uint8_t rom[HCCS_A3K_IDE_ROM_MAX_SIZE];
	size_t rom_size;

	/* Dummy registers as stand-in for VIA; we only care about port A */
	uint8_t via_regs[16];

	/* High-byte latches are separately addressed */
	uint8_t high_byte_latch_r;
	uint8_t high_byte_latch_w;

	ide_t ide;
} hccs_a3k_ide_t;

static void hccs_a3k_ide_irq_raise();
static void hccs_a3k_ide_irq_clear();

static int hccs_a3k_ide_init(struct podule_t *podule)
{
	FILE *f;
	char fn[512];
	char hd4_fn[512] = {0}, hd5_fn[512] = {0};
	int hd_spt[2], hd_hpc[2], hd_cyl[2];
	const char *p;

	hccs_a3k_ide_t *hccs = malloc(sizeof(hccs_a3k_ide_t));
	memset(hccs, 0, sizeof(hccs_a3k_ide_t));

	append_filename(fn, exname, "roms/podules/hccs_a3k_ide/hccs_a3k_ide.rom", 511);
	f = fopen(fn, "rb");
	if (f)
	{
		fread(hccs->rom, HCCS_A3K_IDE_ROM_MAX_SIZE, 1, f);
		hccs->rom_size = ftell(f);
		fclose(f);
	}
	else
	{
		rpclog("hccs_a3k_ide_init failed\n");
		free(hccs);
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

	resetide(&hccs->ide,
		 hd4_fn, hd_spt[0], hd_hpc[0], hd_cyl[0],
		 hd5_fn, hd_spt[1], hd_hpc[1], hd_cyl[1],
		 hccs_a3k_ide_irq_raise, hccs_a3k_ide_irq_clear);

	podule->p = hccs;

	return 0;
}

static void hccs_a3k_ide_reset(struct podule_t *podule)
{
	hccs_a3k_ide_t *hccs = podule->p;

	memset(hccs->via_regs, 0, sizeof(hccs->via_regs));
}

static void hccs_a3k_ide_close(struct podule_t *podule)
{
	hccs_a3k_ide_t *hccs = podule->p;

	closeide(&hccs->ide);
	free(hccs);
}

static void hccs_a3k_ide_irq_raise(ide_t *ide)
{
}

static void hccs_a3k_ide_irq_clear(ide_t *ide)
{
}

static uint8_t hccs_a3k_ide_read_b(struct podule_t *podule, podule_io_type type, uint32_t addr)
{
	hccs_a3k_ide_t *hccs = podule->p;
	int page, romaddr;
	uint8_t temp = 0xff;
	static uint8_t count = 0;

	if (type != PODULE_IO_TYPE_IOC)
		return 0xff; /*Only IOC accesses supported*/

	/* ROM is at its usual spot, welected on !A13. Since the address decoding on
	this card is rather fine-grained, we special-case it here rather than
	putting it in the case statement */
	if (addr < 0x2000) {
		/* Port A of the VIA is the ROM paging latch. If bits of DDRA are 0 (as
		they are on reset), then that bit is treated as an input and pulled
		high, so we OR in the complement of DDRA to simulate this behavior.

		Yes, this means that the ROM starts at the highest page (offset 0x7800
		in a 32K ROM image) */
		page = (hccs->via_regs[VIA_PORTA] | ~hccs->via_regs[VIA_DDRA]) & 0xff;
		romaddr = ((addr & 0x1ffc) | ((page & 0xf) << 13)) >> 2;
		/* Make ROM addresses wrap around based on the size of the ROM image, to
		simulate a smaller ROM in a larger socket */
		temp = hccs->rom[romaddr % hccs->rom_size];
		// rpclog("ide_hccs_a3k: READ PC=%07x addr=%07x -> ROM offset=%04x, page=%02x, romaddr=%07x, val=%02x\n", PC, addr, (addr & 0x1ffc) >> 2, page, romaddr, temp);
	} else {
		 /* Address decoding appears to be on bits A13..A6 */
		switch (addr & 0x3fc0)
		{
			case 0x2000: /* VIA */
				temp = hccs->via_regs[(addr >> 2) & 0xf];
				// rpclog("ide_hccs_a3k: READ PC=%07x addr=%07x -> VIA reg=%04x, val=%02x\n", PC, addr, (addr >> 2) & 0xf, temp);
				break;
			case 0x2100: /* IDE registers */
				if ((addr & 0x1c) == 0)
				{
					uint16_t tempw = readidew(&hccs->ide);

					hccs->high_byte_latch_r = tempw >> 8;
					temp = tempw & 0xff;
					// rpclog("ide_hccs_a3k: READ PC=%07x addr=%07x -> IDE reg=%04x, val=%02x, wval=%04x count=%d\n", PC, addr, (addr >> 2) & 0x7, temp, tempw, count++);
				}
				else
				{
					temp = readide(&hccs->ide, ((addr >> 2) & 7) + 0x1f0);
					// rpclog("ide_hccs_a3k: READ PC=%07x addr=%07x -> IDE reg=%04x, val=%02x\n", PC, addr, (addr >> 2) & 0x7, temp);
				}
				break;
			case 0x2140: /* IDE alternate registers */
				/* Arculator only supports alternate register 6 */
				if (((addr >> 2) & 7) == 6) {
					temp = readide(&hccs->ide, ((addr >> 2) & 7) + 0x3f0);
					// rpclog("ide_hccs_a3k: READ PC=%07x addr=%07x -> IDE alt reg=%04x, val=%02x\n", PC, addr, (addr >> 2) & 0x7, temp);
				} else {
					rpclog("ide_hccs_a3k: READ PC=%07x addr=%07x -> IDE alt reg=%04x (INVALID)\n", PC, addr, (addr >> 2) & 0x7);
				}
				break;
			case 0x2300: /* IDE high byte latch (read) */
				temp = hccs->high_byte_latch_r;
				// rpclog("ide_hccs_a3k: READ PC=%07x addr=%07x -> IDE high byte val=%02x\n", PC, addr, temp);
				break;
			default:
				rpclog("ide_hccs_a3k: READ PC=%07x addr=%07x -> INVALID\n", PC, addr);
				break;
		}
	}
	return temp;
}

static uint16_t hccs_a3k_ide_read_w(struct podule_t *podule, podule_io_type type, uint32_t addr)
{
	return hccs_a3k_ide_read_b(podule, type, addr);
}

static void hccs_a3k_ide_write_b(struct podule_t *podule, podule_io_type type, uint32_t addr, uint8_t val)
{
	hccs_a3k_ide_t *hccs = podule->p;

	if (type != PODULE_IO_TYPE_IOC)
		return; /*Only IOC accesses supported*/

	switch (addr & 0x3fc0)
	{
		case 0x2000: /* VIA */
			// rpclog("ide_hccs_a3k: WRITE PC=%07x addr=%07x -> VIA offset=%04x, val=%02x\n", PC, addr, (addr >> 2) & 0xf, val);
			hccs->via_regs[(addr >> 2) & 0xf] = val;
			break;
		case 0x2100: /* IDE registers */
			if ((addr & 0x1c) == 0)
			{
				// rpclog("ide_hccs_a3k: WRITE PC=%07x addr=%07x -> IDE reg=%04x, val=%02x, wval=%04x\n", PC, addr, (addr >> 2) & 0x7, val, val | (hccs->high_byte_latch_w<<8));
				writeidew(&hccs->ide, val | (hccs->high_byte_latch_w << 8));

			}
			else
			{
				// rpclog("ide_hccs_a3k: WRITE PC=%07x addr=%07x -> IDE reg=%04x, val=%02x\n", PC, addr, (addr >> 2) & 0x7, val);
				writeide(&hccs->ide, ((addr >> 2) & 7) + 0x1f0, val);
			}
			break;
		case 0x2140: /* IDE alternate registers */
			/* Arculator only supports alternate register 6 */
			if (((addr >> 2) & 7) == 6)
			{
				// rpclog("ide_hccs_a3k: WRITE PC=%07x addr=%07x -> IDE alt reg=%04x, val=%02x\n", PC, addr, (addr >> 2) & 0x7, val);
				writeide(&hccs->ide, ((addr >> 2) & 7) + 0x3f0, val);
			} else {
				rpclog("ide_hccs_a3k: WRITE PC=%07x addr=%07x -> IDE alt reg=%04x, val=%02x (INVALID)\n", PC, addr, (addr >> 2) & 0x7, val);
			}
			break;
		case 0x2200: /* IDE high byte latch (write) */
			// rpclog("ide_hccs_a3k: WRITE PC=%07x addr=%07x -> IDE high byte val=%02x\n", PC, addr, val);
			hccs->high_byte_latch_w = val;
			break;
		default:
			rpclog("ide_hccs_a3k: WRITE PC=%07x addr=%07x -> INVALID val=%02x\n", PC, addr, val);
			break;
	}
}

static void hccs_a3k_ide_write_w(struct podule_t *podule, podule_io_type type, uint32_t addr, uint16_t val)
{
	hccs_a3k_ide_write_b(podule, type, addr, val & 0xff);
}

static const podule_header_t hccs_a3k_ide_podule_header =
{
	.version = PODULE_API_VERSION,
	.flags = PODULE_FLAGS_UNIQUE | PODULE_FLAGS_8BIT,
	.short_name = "hccs_a3k_ide",
	.name = "HCCS 8-bit IDE Controller",
	.functions =
	{
		.init = hccs_a3k_ide_init,
		.close = hccs_a3k_ide_close,
		.reset = hccs_a3k_ide_reset,
		.read_b = hccs_a3k_ide_read_b,
		.read_w = hccs_a3k_ide_read_w,
		.write_b = hccs_a3k_ide_write_b,
		.write_w = hccs_a3k_ide_write_w
	},
	.config = &ide_podule_config
};

const podule_header_t *hccs_a3k_ide_probe(const podule_callbacks_t *callbacks, char *path)
{
	FILE *f;
	char fn[512];

	podule_callbacks = callbacks;
	ide_config_init(callbacks);

	append_filename(fn, exname, "roms/podules/hccs_a3k_ide/hccs_a3k_ide.rom", 511);
	f = fopen(fn, "rb");
	if (!f)
		return NULL;
	fclose(f);
	return &hccs_a3k_ide_podule_header;
}
