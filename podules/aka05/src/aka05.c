/*Acorn AKA05 ROM Podule

  IOC address map :
    0000-1fff - ROM
    2000-2fff - latch
      bits 0-5 : ROM A11-A16
      bits 6-7 : ROM select 0-1
    3000-3fff - PAL
      A11 : ROM select 2
      D0 is connected to PAL, but doesn't seem to be used (software stores PC to this address)
*/
//#define DEBUG_LOG

#ifdef WIN32
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include "podule_api.h"

#ifdef WIN32
extern __declspec(dllexport) const podule_header_t *podule_probe(const podule_callbacks_t *callbacks, char *path);
#else
#define BOOL int
#define APIENTRY
#endif

static const podule_callbacks_t *podule_callbacks;
char podule_path[512];

#ifdef DEBUG_LOG
static FILE *aka05_logf;
#endif

void aka05_log(const char *format, ...)
{
#ifdef DEBUG_LOG
   char buf[1024];
//return;
   if (!aka05_logf) aka05_logf=fopen("aka05_log.txt","wt");
   va_list ap;
   va_start(ap, format);
   vsprintf(buf, format, ap);
   va_end(ap);
   fputs(buf,aka05_logf);
   fflush(aka05_logf);
#endif
}

typedef struct aka05_t
{
	uint8_t *roms[8];
	uint32_t rom_mask[8];
	int rom_writable[8];

	int rom_page;
	int rom_select;

	podule_t *podule;
} aka05_t;

static uint8_t aka05_read_b(struct podule_t *podule, podule_io_type type, uint32_t addr)
{
	aka05_t *aka05 = podule->p;
	uint8_t temp = 0xff;

	if (type != PODULE_IO_TYPE_IOC)
		return 0xff;

	//aka05_log("aka05_read_b: addr=%04x\n", addr);
	switch (addr&0x3000)
	{
		case 0x0000: case 0x1000:
		//aka05_log("  rom_select=%i rom_page=%i rom_addr=%04x\n", aka05->rom_select, aka05->rom_page, ((aka05->rom_page * 2048) + ((addr & 0x1fff) >> 2)) & 0x3fff);
		if (aka05->roms[aka05->rom_select])
		{
			uint32_t rom_addr = ((aka05->rom_page * 2048) + ((addr & 0x1fff) >> 2));

			return aka05->roms[aka05->rom_select][rom_addr & aka05->rom_mask[aka05->rom_select]];
		}
		return 0xff; /*No ROM present in this slot*/
	}
	return 0xFF;
}

static void aka05_write_b(struct podule_t *podule, podule_io_type type, uint32_t addr, uint8_t val)
{
	aka05_t *aka05 = podule->p;

	if (type != PODULE_IO_TYPE_IOC)
		return;

	//aka05_log("aka05_write_b: addr=%04x val=%02x\n", addr, val);
	switch (addr & 0x3000)
	{
		case 0x0000: case 0x1000:
		//aka05_log("  rom_select=%i rom_page=%i rom_addr=%04x\n", aka05->rom_select, aka05->rom_page, ((aka05->rom_page * 2048) + ((addr & 0x1fff) >> 2)) & 0x3fff);
		if (aka05->roms[aka05->rom_select] && aka05->rom_writable[aka05->rom_select])
		{
			uint32_t rom_addr = ((aka05->rom_page * 2048) + ((addr & 0x1fff) >> 2));

			aka05->roms[aka05->rom_select][rom_addr & aka05->rom_mask[aka05->rom_select]] = val;
		}
		break;

		case 0x2000:
		aka05->rom_page = val & 0x3f;
		aka05->rom_select = (aka05->rom_select & 4) | ((val >> 6) & 3);
		break;

		case 0x3000:
		if (addr & 0x800)
			aka05->rom_select |= 4;
		else
			aka05->rom_select &= ~4;
		break;
	}
}

static int aka05_init(struct podule_t *podule)
{
	FILE *f;
	char rom_fn[512];
	aka05_t *aka05 = malloc(sizeof(aka05_t));
	memset(aka05, 0, sizeof(aka05_t));

	/*Manager ROM is fixed - and required*/
	sprintf(rom_fn, "%srom_podule_0.07.bin", podule_path);
	aka05_log("aka05 ROM %s\n", rom_fn);
	f = fopen(rom_fn, "rb");
	if (!f)
	{
		aka05_log("Failed to open rom_podule_0.07.bin!\n");
		return -1;
	}
	aka05->roms[0] = malloc(0x4000);
	fread(aka05->roms[0], 0x4000, 1, f);
	aka05->rom_mask[0] = 0x3fff;
	fclose(f);

	/*Remaining ROMs are loaded from config*/
	for (int i = 1; i < 6; i++)
	{
		char config_name[16];

		sprintf(config_name, "rom_fn%i", i+1);

		const char *fn = podule_callbacks->config_get_string(podule, config_name, NULL);

		if (fn)
		{
			aka05_log("aka05 ROM %i %s\n", i, fn);

			f = fopen(fn, "rb");
			if (f)
			{
				uint32_t size, mask;

				fseek(f, -1, SEEK_END);
				size = ftell(f) + 1;
				fseek(f, 0, SEEK_SET);

				if (size <= 0x2000)
					mask = 0x1fff;
				else if (size <= 0x4000)
					mask = 0x3fff;
				else if (size <= 0x8000)
					mask = 0x7fff;
				else if (size <= 0x10000)
					mask = 0xffff;
				else
					mask = 0x1ffff; /*Maximum size = 128k*/

				aka05->roms[i] = malloc(mask+1);
				aka05->rom_mask[i] = mask;
				aka05_log("rom_mask[%i]=%04x\n", i, mask);

				fread(aka05->roms[i], mask+1, 1, f);
				fclose(f);
			}
		}
	}

	if (podule_callbacks->config_get_int(podule, "ram_7", 0))
	{
		aka05->roms[6] = malloc(0x8000);
		aka05->rom_mask[6] = 0x7fff;
		aka05->rom_writable[6] = 1;
	}
	if (podule_callbacks->config_get_int(podule, "ram_8", 0))
	{
		aka05->roms[7] = malloc(0x8000);
		aka05->rom_mask[7] = 0x7fff;
		aka05->rom_writable[7] = 1;
	}

	aka05->rom_select = 0;

	aka05->podule = podule;
	podule->p = aka05;

	return 0;
}

static void aka05_close(struct podule_t *podule)
{
	aka05_t *aka05 = podule->p;

	for (int i = 0; i < 8; i++)
	{
		if (aka05->roms[i])
			free(aka05->roms[i]);
	}

	free(aka05);
}

enum
{
	ID_ROM_2, ID_LOAD_ROM_2,
	ID_ROM_3, ID_LOAD_ROM_3,
	ID_ROM_4, ID_LOAD_ROM_4,
	ID_ROM_5, ID_LOAD_ROM_5,
	ID_ROM_6, ID_LOAD_ROM_6,
	ID_RAM_7,
	ID_RAM_8
};

static int config_load_rom(void *window_p, const struct podule_config_item_t *item, void *new_data)
{
	char fn[512];

	if (!podule_callbacks->config_file_selector(window_p, "Please select a ROM image",
			NULL, NULL, NULL, "ROM files (*.rom)|*.rom", fn, sizeof(fn), CONFIG_FILESEL_LOAD))
	{
		int fn_id = item->id - 1;

		podule_callbacks->config_set_current(window_p, fn_id, fn);

		return 1;
	}

	return 0;
}

static podule_config_t aka05_config =
{
	.items =
	{
		{
			.name = "rom_fn2",
			.description = "ROM 2:",
			.type = CONFIG_STRING,
			.flags = 0,
			.id = ID_ROM_2
		},
		{
			.description = "...",
			.type = CONFIG_BUTTON,
			.function = config_load_rom,
			.id = ID_LOAD_ROM_2,
		},
		{
			.name = "rom_fn3",
			.description = "ROM 3:",
			.type = CONFIG_STRING,
			.flags = 0,
			.id = ID_ROM_3
		},
		{
			.description = "...",
			.type = CONFIG_BUTTON,
			.function = config_load_rom,
			.id = ID_LOAD_ROM_3,
		},
		{
			.name = "rom_fn4",
			.description = "ROM 4:",
			.type = CONFIG_STRING,
			.flags = 0,
			.id = ID_ROM_4
		},
		{
			.description = "...",
			.type = CONFIG_BUTTON,
			.function = config_load_rom,
			.id = ID_LOAD_ROM_4,
		},
		{
			.name = "rom_fn5",
			.description = "ROM 5:",
			.type = CONFIG_STRING,
			.flags = 0,
			.id = ID_ROM_5
		},
		{
			.description = "...",
			.type = CONFIG_BUTTON,
			.function = config_load_rom,
			.id = ID_LOAD_ROM_5
		},
		{
			.name = "rom_fn6",
			.description = "ROM 6:",
			.type = CONFIG_STRING,
			.flags = 0,
			.id = ID_ROM_6
		},
		{
			.description = "...",
			.type = CONFIG_BUTTON,
			.function = config_load_rom,
			.id = ID_LOAD_ROM_6
		},
		{
			.name = "ram_7",
			.description = "32k RAM in Slot 7",
			.type = CONFIG_BINARY,
			.flags = 0,
			.id = ID_RAM_7
		},
		{
			.name = "ram_8",
			.description = "32k RAM in Slot 8",
			.type = CONFIG_BINARY,
			.flags = 0,
			.id = ID_RAM_8
		},
		{
			.type = -1
		}
	}
};

static const podule_header_t aka05_podule_header =
{
	.version = PODULE_API_VERSION,
	.flags = 0,
	.short_name = "aka05",
	.name = "Acorn AKA05 ROM Podule",
	.functions =
	{
		.init = aka05_init,
		.close = aka05_close,
		.read_b = aka05_read_b,
		.write_b = aka05_write_b
	},
	.config = &aka05_config
};

const podule_header_t *podule_probe(const podule_callbacks_t *callbacks, char *path)
{
	podule_callbacks = callbacks;
	strcpy(podule_path, path);

	return &aka05_podule_header;
}

#ifdef WIN32
BOOL APIENTRY DllMain (HINSTANCE hInst     /* Library instance handle. */ ,
		       DWORD reason        /* Reason this function is being called. */ ,
		       LPVOID reserved     /* Not used. */ )
{
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
