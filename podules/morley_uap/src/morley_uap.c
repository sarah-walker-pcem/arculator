/*Morley A3000 User and Analogue Port

  IOC address map :
  0000-1fff - ROM
  2000-2fff - 6522 VIA
    PA0-PA3 - ROM bank
    PA6 - Analogue fire button 0
    PA7 - Analogue fire button 1
    PB0-7, CB1, CB2 - User port

  MEMC address map :
  1000-1fff - ADC

  Expansion card header is at ROM address 0x3800 (ROM bank 7). VIA port A is set to all input on reset, which
  results in the ROM high bits being pulled high. The loader will switch PA0-PA2 between input to read the
  header and chunk directory, and output to read module data.

  ADC and VIA are currently implemented. User port is not.
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
#include "6522.h"
#include "d7002c.h"
#include "joystick_api.h"
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
static FILE *morley_uap_logf;
#endif

void morley_uap_log(const char *format, ...)
{
#ifdef DEBUG_LOG
   char buf[1024];
//return;
   if (!morley_uap_logf) morley_uap_logf=fopen("morley_uap_log.txt","wt");
   va_list ap;
   va_start(ap, format);
   vsprintf(buf, format, ap);
   va_end(ap);
   fputs(buf,morley_uap_logf);
   fflush(morley_uap_logf);
#endif
}

typedef struct morley_uap_t
{
	uint8_t rom[0x4000];
	int rom_page;

	int adc_irq;
	int via_irq;

	d7002c_t d7002c;
	via6522_t via6522;

	int joy_poll_count;

	podule_t *podule;
} morley_uap_t;

static uint8_t morley_uap_read_b(struct podule_t *podule, podule_io_type type, uint32_t addr)
{
	morley_uap_t *morley_uap = podule->p;
	uint8_t temp = 0xff;

	if (type == PODULE_IO_TYPE_MEMC)
	{
//		morley_uap_log("morley_uap_read_b: MEMC, addr=%04x\n", addr);

		switch (addr & 0x3000)
		{
			case 0x1000: /*D7002C ADC*/
			return d7002c_read(&morley_uap->d7002c, addr >> 2);
		}
	}
	else
	{
//		if ((addr & 0x3fff) >= 0x2010)
//			morley_uap_log("morley_uap_read_b: addr=%04x\n", addr);
		switch (addr&0x3000)
		{
			case 0x0000: case 0x1000:
			//morley_uap_log("  rom_page=%i rom_addr=%04x\n", morley_uap->rom_page, ((morley_uap->rom_page * 2048) + ((addr & 0x1fff) >> 2)) & 0x3fff);
			return morley_uap->rom[((morley_uap->rom_page * 2048) + ((addr & 0x1fff) >> 2)) & 0x3fff];

			case 0x2000:
			return via6522_read(&morley_uap->via6522, addr >> 2);
		}
	}
	return 0xFF;
}

static void morley_uap_write_b(struct podule_t *podule, podule_io_type type, uint32_t addr, uint8_t val)
{
	morley_uap_t *morley_uap = podule->p;

	if (type == PODULE_IO_TYPE_MEMC)
	{
//		morley_uap_log("morley_uap_write_b: MEMC, addr=%04x val=%02x\n", addr, val);
		switch (addr & 0x3000)
		{
			case 0x1000: /*D7002C ADC*/
//			morley_uap_log("ADC write %08x %02x\n", addr, val);
			d7002c_write(&morley_uap->d7002c, addr >> 2, val);
			break;
		}
	}
	else
	{
//		if ((addr & 0x3fff) >= 0x2010)
//			morley_uap_log("morley_uap_write_b: addr=%04x val=%02x\n", addr, val);
		switch (addr & 0x3000)
		{
			case 0x2000:
			via6522_write(&morley_uap->via6522, addr >> 2, val);
			break;
		}
	}
}

static int morley_uap_run(struct podule_t *podule, int timeslice_us)
{
	morley_uap_t *morley_uap = podule->p;

	morley_uap->joy_poll_count++;
	if (morley_uap->joy_poll_count >= 20)
	{
		morley_uap->joy_poll_count = 0;
		/*Approximately 20ms*/
		joystick_poll_host();
	}

	d7002c_poll(&morley_uap->d7002c);
	via6522_updatetimers(&morley_uap->via6522, timeslice_us*2);

	return 1000; /*1ms*/
}


static uint8_t morley_uap_via_read_portA(void *p)
{
	morley_uap_t *morley_uap = p;
	uint8_t temp = 0xff;

	if (joystick_state[0].button[0])
		temp &= ~0x40;
	if (joystick_state[1].button[0])
		temp &= ~0x80;

	return temp;
}

static void morley_uap_via_write_portA(void *p, uint8_t val)
{
	morley_uap_t *morley_uap = p;

	morley_uap->rom_page = val & 7;
}

static void morley_uap_adc_irq(void *p, int state)
{
	morley_uap_t *morley_uap = p;
	podule_t *podule = morley_uap->podule;

	morley_uap->adc_irq = state;
	podule_callbacks->set_irq(podule, morley_uap->adc_irq | morley_uap->via_irq);
}

static void morley_uap_via_irq(void *p, int state)
{
	morley_uap_t *morley_uap = p;
	podule_t *podule = morley_uap->podule;

	morley_uap->via_irq = state;
	podule_callbacks->set_irq(podule, morley_uap->adc_irq | morley_uap->via_irq);
}

int joystick_get_max_joysticks(void)
{
	return 2;
}

int joystick_get_axis_count(void)
{
	return 2;
}

int joystick_get_button_count(void)
{
	return 1;
}

int joystick_get_pov_count(void)
{
	return 0;
}

static int morley_uap_init(struct podule_t *podule)
{
	FILE *f;
	char rom_fn[512];
	morley_uap_t *morley_uap = malloc(sizeof(morley_uap_t));
	memset(morley_uap, 0, sizeof(morley_uap_t));

	sprintf(rom_fn, "%sMorleyIO.BIN", podule_path);
	morley_uap_log("morley_uap ROM %s\n", rom_fn);
	f = fopen(rom_fn, "rb");
	if (!f)
	{
		morley_uap_log("Failed to open morley_uap.ROM!\n");
		return -1;
	}
	fread(morley_uap->rom, 0x4000, 1, f);
	fclose(f);

	morley_uap->rom_page = 7; /*Header is in last page*/

	d7002c_init(&morley_uap->d7002c, morley_uap_adc_irq, morley_uap);
	via6522_init(&morley_uap->via6522, morley_uap_via_irq, morley_uap);
	morley_uap->via6522.read_portA = morley_uap_via_read_portA;
	morley_uap->via6522.write_portA = morley_uap_via_write_portA;

	morley_uap->podule = podule;
	podule->p = morley_uap;

	joystick_init(podule, podule_callbacks);

	return 0;
}

static void morley_uap_close(struct podule_t *podule)
{
	morley_uap_t *morley_uap = podule->p;

	joystick_close();

	free(morley_uap);
}

static podule_config_t joystick_2a_1b_config =
{
	.items =
	{
		{
			.name = "axis_0",
			.description = "X axis",
			.type = CONFIG_SELECTION,
			.flags = CONFIG_FLAGS_NAME_PREFIXED,
			.selection = joystick_axis_config_selection,
			.default_int = 0,
			.id = 1
		},
		{
			.name = "axis_1",
			.description = "Y axis",
			.type = CONFIG_SELECTION,
			.flags = CONFIG_FLAGS_NAME_PREFIXED,
			.selection = joystick_axis_config_selection,
			.default_int = 1,
			.id = 2
		},
		{
			.name = "button_0",
			.description = "Fire button",
			.type = CONFIG_SELECTION,
			.flags = CONFIG_FLAGS_NAME_PREFIXED,
			.selection = joystick_button_config_selection,
			.default_int = 0,
			.id = 3
		},
		{
			.type = -1
		}
	}
};

enum
{
	ID_JOYSTICK_0_DEVICE,
	ID_JOYSTICK_1_DEVICE,
	ID_JOYSTICK_0_MAPPING,
	ID_JOYSTICK_1_MAPPING
};

static int config_joystick(void *window_p, const struct podule_config_item_t *item, void *new_data)
{
	int joystick_nr;
	int device;
	char prefix[32];

	joystick_nr = (item->id == ID_JOYSTICK_0_MAPPING) ? 0 : 1;
	device = (int)podule_callbacks->config_get_current(window_p, joystick_nr ? ID_JOYSTICK_1_DEVICE : ID_JOYSTICK_0_DEVICE);

	if (device)
	{
		joystick_update_axes_config(device);
		joystick_update_buttons_config(device);

		sprintf(prefix, "joystick_%i_", joystick_nr);
		podule_callbacks->config_open(window_p, &joystick_2a_1b_config, prefix);
	}

	return 0;
}

static podule_config_t morley_uap_config =
{
	.items =
	{
		{
			.name = "joystick_0_nr",
			.description = "Device",
			.type = CONFIG_SELECTION,
			.selection = NULL,
			.default_int = 0,
			.id = ID_JOYSTICK_0_DEVICE
		},
		{
			.description = "Joystick 1...",
			.type = CONFIG_BUTTON,
			.function = config_joystick,
			.id = ID_JOYSTICK_0_MAPPING
		},
		{
			.name = "joystick_1_nr",
			.description = "Device",
			.type = CONFIG_SELECTION,
			.selection = NULL,
			.default_int = 0,
			.id = ID_JOYSTICK_1_DEVICE
		},
		{
			.description = "Joystick 2...",
			.type = CONFIG_BUTTON,
			.function = config_joystick,
			.id = ID_JOYSTICK_1_MAPPING
		},
		{
			.type = -1
		}
	}
};

static const podule_header_t morley_uap_podule_header =
{
	.version = PODULE_API_VERSION,
	.flags = PODULE_FLAGS_UNIQUE | PODULE_FLAGS_8BIT,
	.short_name = "morley_uap",
	.name = "Morley A3000 User and Analogue Port",
	.functions =
	{
		.init = morley_uap_init,
		.close = morley_uap_close,
		.read_b = morley_uap_read_b,
		.write_b = morley_uap_write_b,
		.run = morley_uap_run
	},
	.config = &morley_uap_config
};

const podule_header_t *podule_probe(const podule_callbacks_t *callbacks, char *path)
{
	podule_callbacks = callbacks;
	strcpy(podule_path, path);

	morley_uap_config.items[0].selection = joystick_devices_config(callbacks);
	morley_uap_config.items[2].selection = joystick_devices_config(callbacks);

	return &morley_uap_podule_header;
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
