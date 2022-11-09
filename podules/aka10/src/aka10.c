/*Acorn AKA10/15 IO/MIDI Podule

  IOC address map :
  0000-1fff - ROM
  2000-2fff - 6522 VIA
    PA0-PA2 - ROM bank
    PA3 - EPROM PGM
    PA4 - EPROM Vpp
    PA6 - Analogue fire button 0
    PA7 - Analogue fire button 1
    CA1 - Analogue LPSTB
    CA2 - !IRQs enabled
    PB0-7, CB1, CB2 - User port

  MEMC address map :
  0000-07ff - FRED
  0800-0fff - JIM
  1000-17ff - ADC
  1800-1fff - 6850 UART

  Expansion card header is at ROM address 0x3800 (ROM bank 7). VIA port A is set to all input on reset, which
  results in the ROM high bits being pulled high. The loader will switch PA0-PA2 between input to read the
  header and chunk directory, and output to read module data.

  MIDI, ADC and VIA are currently implemented. User port and 1 MHz bus are not.
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
#include "6850.h"
#include "d7002c.h"
#include "joystick_api.h"
#include "midi.h"
#include "podule_api.h"

#ifdef WIN32
extern __declspec(dllexport) const podule_header_t *podule_probe(const podule_callbacks_t *callbacks, char *path);
#else
#define BOOL int
#define APIENTRY
#endif

static const podule_callbacks_t *podule_callbacks;
char podule_path[512];

#define MIDI_UART_CLOCK 2000000 //(31250Hz * 4 * 16)

#ifdef DEBUG_LOG
static FILE *aka10_logf;
#endif

void aka10_log(const char *format, ...)
{
#ifdef DEBUG_LOG
   char buf[1024];
//return;
   if (!aka10_logf) aka10_logf=fopen("aka10_log.txt","wt");
   va_list ap;
   va_start(ap, format);
   vsprintf(buf, format, ap);
   va_end(ap);
   fputs(buf,aka10_logf);
   fflush(aka10_logf);
#endif
}

typedef struct aka10_t
{
	uint8_t rom[0x4000];
	int rom_page;

	uint8_t ora, ddra;

	int adc_irq;
	int uart_irq;
	int irqs_enabled;

	d7002c_t d7002c;
	m6850_t m6850;
	via6522_t via6522;

	int joy_poll_count;
	int ms_poll_count;

	podule_t *podule;
	void *midi;
} aka10_t;

static uint8_t aka10_read_b(struct podule_t *podule, podule_io_type type, uint32_t addr)
{
	aka10_t *aka10 = podule->p;
	uint8_t temp = 0xff;

	if (type == PODULE_IO_TYPE_MEMC)
	{
//		aka10_log("aka10_read_b: MEMC, addr=%04x\n", addr);
		switch (addr & 0x3800)
		{
			case 0x1000: /*D7002C ADC*/
//			aka10_log("ADC read %08x\n", addr);
			return d7002c_read(&aka10->d7002c, addr >> 2);
			case 0x1800: /*6850 UART*/
			return m6850_read(&aka10->m6850, addr >> 2);
		}
	}
	else
	{
//		if ((addr & 0x3fff) >= 0x2010)
//			aka10_log("aka10_read_b: addr=%04x\n", addr);
		switch (addr&0x3000)
		{
			case 0x0000: case 0x1000:
			//aka10_log("  rom_page=%i rom_addr=%04x\n", aka10->rom_page, ((aka10->rom_page * 2048) + ((addr & 0x1fff) >> 2)) & 0x3fff);
			return aka10->rom[((aka10->rom_page * 2048) + ((addr & 0x1fff) >> 2)) & 0x3fff];

			case 0x2000:
			return via6522_read(&aka10->via6522, addr >> 2);
		}
	}
	return 0xFF;
}

static void aka10_write_b(struct podule_t *podule, podule_io_type type, uint32_t addr, uint8_t val)
{
	aka10_t *aka10 = podule->p;

	if (type == PODULE_IO_TYPE_MEMC)
	{
//		aka10_log("aka10_write_b: MEMC, addr=%04x val=%02x\n", addr, val);
		switch (addr & 0x3800)
		{
			case 0x1000: /*D7002C ADC*/
//			aka10_log("ADC write %08x %02x\n", addr, val);
			d7002c_write(&aka10->d7002c, addr >> 2, val);
			break;
			case 0x1800: /*6850 UART*/
			m6850_write(&aka10->m6850, addr >> 2, val);
			break;
		}
	}
	else
	{
		if ((addr & 0x3fff) >= 0x2010)
			aka10_log("aka10_write_b: addr=%04x val=%02x\n", addr, val);
		switch (addr & 0x3000)
		{
			case 0x2000:
			via6522_write(&aka10->via6522, addr >> 2, val);
			break;
		}
	}
}

static int aka10_run(struct podule_t *podule, int timeslice_us)
{
	aka10_t *aka10 = podule->p;

	aka10->ms_poll_count++;
	if (aka10->ms_poll_count >= 4)
	{
		aka10->ms_poll_count = 0;
		/*Approximately 1ms*/
		d7002c_poll(&aka10->d7002c);
	}
	aka10->joy_poll_count++;
	if (aka10->joy_poll_count >= 80)
	{
		aka10->joy_poll_count = 0;
		/*Approximately 20ms*/
		joystick_poll_host();
	}

	via6522_updatetimers(&aka10->via6522, timeslice_us*2);
	m6850_run(&aka10->m6850, timeslice_us);

	return 256; /*256us = 1 byte at 31250 baud*/
}

static void aka10_update_irqs(aka10_t *aka10)
{
	podule_t *podule = aka10->podule;

	if ((aka10->adc_irq || aka10->uart_irq) && aka10->irqs_enabled)
		podule_callbacks->set_irq(podule, 1);
	else
		podule_callbacks->set_irq(podule, 0);
}

static void aka10_adc_irq(void *p, int state)
{
	aka10_t *aka10 = p;
	podule_t *podule = aka10->podule;

	aka10->adc_irq = state;
	aka10_update_irqs(aka10);
}

static void aka10_uart_irq(void *p, int state)
{
	aka10_t *aka10 = p;
	podule_t *podule = aka10->podule;

	aka10->uart_irq = state;
	aka10_update_irqs(aka10);
}

static void aka10_via_irq(void *p, int state)
{
	/*Not connected by default*/
}

static uint8_t aka10_via_read_portA(void *p)
{
	aka10_t *aka10 = p;
	uint8_t temp = 0xff;

	if (joystick_state[0].button[0])
		temp &= ~0x40;
	if (joystick_state[1].button[0])
		temp &= ~0x80;

	return temp;
}

static void aka10_via_write_portA(void *p, uint8_t val)
{
	aka10_t *aka10 = p;

	aka10->rom_page = val & 7;
}

static void aka10_via_set_ca2(void *p, int level)
{
	aka10_t *aka10 = p;

	aka10->irqs_enabled = !level;
	aka10_update_irqs(aka10);
}

static void aka10_uart_send(void *p, uint8_t val)
{
	struct aka10_t *aka10 = p;

	midi_write(aka10->midi, val);
}

static void aka10_midi_receive(void *p, uint8_t val)
{
	struct aka10_t *aka10 = p;

	m6850_receive(&aka10->m6850, val);
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

static int aka10_init(struct podule_t *podule)
{
	FILE *f;
	char rom_fn[512];
	aka10_t *aka10 = malloc(sizeof(aka10_t));
	memset(aka10, 0, sizeof(aka10_t));

	sprintf(rom_fn, "%saka10.rom", podule_path);
	aka10_log("aka10 ROM %s\n", rom_fn);
	f = fopen(rom_fn, "rb");
	if (!f)
	{
		aka10_log("Failed to open aka10.ROM!\n");
		return -1;
	}
	fread(aka10->rom, 0x4000, 1, f);
	fclose(f);

	aka10->rom_page = 7; /*Header is in last page*/

	d7002c_init(&aka10->d7002c, aka10_adc_irq, aka10);
	m6850_init(&aka10->m6850, MIDI_UART_CLOCK, aka10_uart_irq, aka10_uart_send, aka10, aka10_log);
	via6522_init(&aka10->via6522, aka10_via_irq, aka10);
	aka10->via6522.read_portA = aka10_via_read_portA;
	aka10->via6522.write_portA = aka10_via_write_portA;
	aka10->via6522.set_ca2 = aka10_via_set_ca2;

	aka10->midi = midi_init(aka10, aka10_midi_receive, aka10_log, podule_callbacks, podule);

	aka10->podule = podule;
	podule->p = aka10;

	joystick_init(podule, podule_callbacks);

	return 0;
}

static void aka10_close(struct podule_t *podule)
{
	aka10_t *aka10 = podule->p;

	joystick_close();

	midi_close(aka10->midi);

	free(aka10);
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
	ID_MIDI_OUT_DEVICE,
	ID_MIDI_IN_DEVICE,
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

static podule_config_t aka10_config =
{
	.items =
	{
		{
			.name = "midi_out_device",
			.description = "MIDI output device",
			.type = CONFIG_SELECTION,
			.selection = NULL,
			.default_int = -1,
			.id = ID_MIDI_OUT_DEVICE
		},
		{
			.name = "midi_in_device",
			.description = "MIDI input device",
			.type = CONFIG_SELECTION,
			.selection = NULL,
			.default_int = -1,
			.id = ID_MIDI_IN_DEVICE
		},
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

static const podule_header_t aka10_podule_header =
{
	.version = PODULE_API_VERSION,
	.flags = PODULE_FLAGS_UNIQUE,
	.short_name = "aka10",
	.name = "Acorn AKA10/15 IO/MIDI Podule",
	.functions =
	{
		.init = aka10_init,
		.close = aka10_close,
		.read_b = aka10_read_b,
		.write_b = aka10_write_b,
		.run = aka10_run
	},
	.config = &aka10_config
};

const podule_header_t *podule_probe(const podule_callbacks_t *callbacks, char *path)
{
	podule_callbacks = callbacks;
	strcpy(podule_path, path);

	aka10_config.items[0].selection = midi_out_devices_config();
	aka10_config.items[1].selection = midi_in_devices_config();

	aka10_config.items[2].selection = joystick_devices_config(callbacks);
	aka10_config.items[4].selection = joystick_devices_config(callbacks);

	return &aka10_podule_header;
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
