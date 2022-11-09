/*Acorn AKA12 User Port / MIDI Upgrade

  IOC address map :
  0000-1fff - ROM
  2000-2fff - 6522 VIA
    PA0-PA2 - ROM bank
  3000-3fff - SCC2691 UART

  Expansion card header is at ROM address 0x3800 (ROM bank 7). VIA port A is set to all input on reset, which
  results in the ROM high bits being pulled high. The loader will switch PA0-PA2 between input to read the
  header and chunk directory, and output to read module data.

  Both UART and VIA are connected to podule IRQ.
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
#include "aka12.h"
#include "scc2691.h"
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
static FILE *aka12_logf;
#endif

void aka12_log(const char *format, ...)
{
#ifdef DEBUG_LOG
   char buf[1024];
//return;
   if (!aka12_logf) aka12_logf=fopen("aka12_log.txt","wt");
   va_list ap;
   va_start(ap, format);
   vsprintf(buf, format, ap);
   va_end(ap);
   fputs(buf,aka12_logf);
   fflush(aka12_logf);
#endif
}

typedef struct aka12_t
{
	uint8_t rom[0x4000];
	int rom_page;

	scc2691_t scc2691;
	via6522_t via6522;

	int uart_irq;
	int via_irq;

	podule_t *podule;
	void *midi;
} aka12_t;

static uint8_t aka12_read_b(struct podule_t *podule, podule_io_type type, uint32_t addr)
{
	aka12_t *aka12 = podule->p;
	uint8_t temp = 0xff;

	if (type != PODULE_IO_TYPE_IOC)
		return 0xff;

	//aka12_log("aka12_read_b: addr=%04x\n", addr);
	switch (addr&0x3000)
	{
		case 0x0000: case 0x1000:
		//aka12_log("  rom_page=%i rom_addr=%04x\n", aka12->rom_page, ((aka12->rom_page * 2048) + ((addr & 0x1fff) >> 2)) & 0x3fff);
		return aka12->rom[((aka12->rom_page * 2048) + ((addr & 0x1fff) >> 2)) & 0x3fff];

		case 0x2000:
		return via6522_read(&aka12->via6522, addr >> 2);

		case 0x3000:
		return scc2691_read(&aka12->scc2691, (addr >> 2) & 7);
	}
	return 0xFF;
}

static void aka12_write_b(struct podule_t *podule, podule_io_type type, uint32_t addr, uint8_t val)
{
	aka12_t *aka12 = podule->p;

	if (type != PODULE_IO_TYPE_IOC)
		return;

//	aka12_log("aka12_write_b: addr=%04x val=%02x\n", addr, val);
	switch (addr & 0x3000)
	{
		case 0x2000:
		via6522_write(&aka12->via6522, addr >> 2, val);
		break;

		case 0x3000: /*SCC2691*/
		scc2691_write(&aka12->scc2691, (addr >> 2) & 7, val);
		break;
	}
}

static int aka12_run(struct podule_t *podule, int timeslice_us)
{
	aka12_t *aka12 = podule->p;

	scc2691_run(&aka12->scc2691, timeslice_us);
	via6522_updatetimers(&aka12->via6522, timeslice_us*2);

	return 256; /*256us = 1 byte at 31250 baud*/
}

static void aka12_uart_irq(void *p, int state)
{
	aka12_t *aka12 = p;
	podule_t *podule = aka12->podule;

	aka12->uart_irq = state;
	podule_callbacks->set_irq(podule, aka12->uart_irq | aka12->via_irq);
}

static void aka12_via_irq(void *p, int state)
{
	aka12_t *aka12 = p;
	podule_t *podule = aka12->podule;

	aka12->via_irq = state;
	podule_callbacks->set_irq(podule, aka12->uart_irq | aka12->via_irq);
}

static void aka12_via_write_portA(void *p, uint8_t val)
{
	aka12_t *aka12 = p;

	aka12->rom_page = val & 7;
}

static void aka12_uart_send(void *p, uint8_t val)
{
	struct aka12_t *aka12 = p;

	midi_write(aka12->midi, val);
}

static void aka12_midi_receive(void *p, uint8_t val)
{
	struct aka12_t *aka12 = p;

	scc2691_receive(&aka12->scc2691, val);
}

static int aka12_init(struct podule_t *podule)
{
	FILE *f;
	char rom_fn[512];
	aka12_t *aka12 = malloc(sizeof(aka12_t));
	memset(aka12, 0, sizeof(aka12_t));

	sprintf(rom_fn, "%saka12.rom", podule_path);
	aka12_log("aka12 ROM %s\n", rom_fn);
	f = fopen(rom_fn, "rb");
	if (!f)
	{
		aka12_log("Failed to open aka12.ROM!\n");
		return -1;
	}
	fread(aka12->rom, 0x4000, 1, f);
	fclose(f);

	aka12->rom_page = 7; /*Header is in last page*/

	scc2691_init(&aka12->scc2691, MIDI_UART_CLOCK, aka12_uart_irq, aka12_uart_send, aka12, aka12_log);
	via6522_init(&aka12->via6522, aka12_via_irq, aka12);
	aka12->via6522.write_portA = aka12_via_write_portA;

	aka12->midi = midi_init(aka12, aka12_midi_receive, aka12_log, podule_callbacks, podule);

	aka12->podule = podule;
	podule->p = aka12;

	return 0;
}

static void aka12_close(struct podule_t *podule)
{
	aka12_t *aka12 = podule->p;

	midi_close(aka12->midi);

	free(aka12);
}

static podule_config_t aka12_config =
{
	.items =
	{
		{
			.name = "midi_out_device",
			.description = "MIDI output device",
			.type = CONFIG_SELECTION,
			.selection = NULL,
			.default_int = -1
		},
		{
			.name = "midi_in_device",
			.description = "MIDI input device",
			.type = CONFIG_SELECTION,
			.selection = NULL,
			.default_int = -1
		},
		{
			.type = -1
		}
	}
};

static const podule_header_t aka12_podule_header =
{
	.version = PODULE_API_VERSION,
	.flags = PODULE_FLAGS_UNIQUE | PODULE_FLAGS_8BIT,
	.short_name = "aka12",
	.name = "Acorn AKA12 User Port / MIDI Upgrade",
	.functions =
	{
		.init = aka12_init,
		.close = aka12_close,
		.read_b = aka12_read_b,
		.write_b = aka12_write_b,
		.run = aka12_run
	},
	.config = &aka12_config
};

const podule_header_t *podule_probe(const podule_callbacks_t *callbacks, char *path)
{
	podule_callbacks = callbacks;
	strcpy(podule_path, path);

	aka12_config.items[0].selection = midi_out_devices_config();
	aka12_config.items[1].selection = midi_in_devices_config();

	return &aka12_podule_header;
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
