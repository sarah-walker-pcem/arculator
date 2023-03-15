/*Design IT E200+ Access+ podule

  IOC address map :
    000-7ff - ROM (read)
	      Page register (write)
	        A2 is top bit of page register
    800-fff - IRQ status (read)
	        bit 0 - IRQ active

  MEMC address map :
    000-7ff - MX98902QC registers
	        Arc A2-A5 maps to A0-A3
    800-fff - Remote DMA transfer port
*/

#ifdef WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>
#include "podule_api.h"
#include "designit_e200.h"
#include "net.h"
#include "ne2000.h"

#ifdef WIN32
extern __declspec(dllexport) const podule_header_t *podule_probe(const podule_callbacks_t *callbacks, char *path);
#else
#define BOOL int
#define APIENTRY
#endif

static const podule_callbacks_t *podule_callbacks;
static char podule_path[512];

typedef struct e200_t
{
	uint8_t rom[0x40000];

	uint16_t page_reg;

	int irq_status;

	void *ne2000;

	podule_t *podule;

	net_t *net;
} e200_t;

static FILE *e200_logf;
static int timestamp = 0;
void e200_log(const char *format, ...)
{
   	char buf[1024];
   	va_list ap;

	return;

	if (!e200_logf)
		e200_logf = fopen("e200_log.txt", "wt");

   	va_start(ap, format);
   	vsprintf(buf, format, ap);
   	va_end(ap);
   	fprintf(e200_logf, "[%08i] : ", timestamp);
   	fputs(buf, e200_logf);
   	fflush(e200_logf);
}
void e200_fatal(const char *format, ...)
{
   	char buf[1024];
   	va_list ap;

	return;

	if (!e200_logf)
		e200_logf = fopen("e200_log.txt", "wt");

   	va_start(ap, format);
   	vsprintf(buf, format, ap);
   	va_end(ap);
   	fprintf(e200_logf, "[%08i] : ", timestamp);
   	fputs(buf, e200_logf);
   	fflush(e200_logf);
   	exit(-1);
}

static uint8_t e200_read_b(struct podule_t *podule, podule_io_type type, uint32_t addr)
{
	e200_t *e200 = podule->p;

	if (type == PODULE_IO_TYPE_IOC)
	{
		switch (addr & 0x800)
		{
			case 0x000:
			return e200->rom[((addr >> 2) & 0x1ff) | (e200->page_reg << 9)];

			case 0x800:
			return e200->irq_status ? 1 : 0;
		}
	}
	else
	{
		uint8_t ret;

		switch (addr & 0x800)
		{
			case 0x000:
			ret = ne2000_read((addr >> 2) & 0xf, e200->ne2000);
			break;

			case 0x800:
			ret = ne2000_dma_read_w(addr, e200->ne2000);
			break;
		}

		return ret;
	}
}

static uint16_t e200_read_w(struct podule_t *podule, podule_io_type type, uint32_t addr)
{
	return e200_read_b(podule, type, addr) | 0xff00;
}

static void e200_write_b(struct podule_t *podule, podule_io_type type, uint32_t addr, uint8_t val)
{
	e200_t *e200 = podule->p;

	if (type == PODULE_IO_TYPE_IOC)
	{
		switch (addr & 0x800)
		{
			case 0x000:
			e200->page_reg = val | ((addr & 4) << 6);
			break;
		}
	}
	else
	{
		switch (addr & 0x800)
		{
			case 0x000:
			ne2000_write((addr >> 2) & 0xf, val, e200->ne2000);
			break;

			case 0x800:
			ne2000_dma_write_w(addr, val | 0xff00, e200->ne2000);
			break;
		}
	}
}

static void e200_write_w(struct podule_t *podule, podule_io_type type, uint32_t addr, uint16_t val)
{
	e200_write_b(podule, type, addr, val & 0xff);
}


static void e200_reset(struct podule_t *podule)
{
	e200_t *e200 = podule->p;

	e200_log("Reset e200\n");

	e200->page_reg = 0;
}

void e200_set_irq(void *p, int irq)
{
	e200_t *e200 = p;

	podule_callbacks->set_irq(e200->podule, irq);
	e200->irq_status = irq ? 1 : 0;
}

static int e200_init(struct podule_t *podule)
{
	FILE *f;
	char rom_fn[512];
	uint8_t mac[6] = {0x00, 0x02, 0x07, 0x00, 0xda, 0x98};
	e200_t *e200 = malloc(sizeof(e200_t));
	memset(e200, 0, sizeof(e200_t));

	sprintf(rom_fn, "%sDesign IT E200 Access+.rom", podule_path);
	e200_log("e200 ROM %s\n", rom_fn);
	f = fopen(rom_fn, "rb");
	if (!f)
	{
		e200_log("Failed to open Design IT E200 Access+.rom!\n");
		return -1;
	}
	fread(e200->rom, 0x40000, 1, f);
	fclose(f);

	e200->podule = podule;
	podule->p = e200;

	sscanf(&e200->rom[0x154], "%02x:%02x:%02x:%02x:%02x:%02x", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);

	const char *network_device = podule_callbacks->config_get_string(podule, "network_device", NETWORK_DEVICE_DEFAULT);
	e200->net = net_init(network_device, mac);

	e200->ne2000 = ne2000_init(e200_set_irq, e200, e200->net);

	e200_log("e200_init: podule=%p\n", podule);
	return 0;
}

static void e200_close(struct podule_t *podule)
{
	e200_t *e200 = podule->p;

	e200->net->close((struct net_t *)e200->net);

	ne2000_close(e200->ne2000);

	free(e200);
}

static int e200_run(struct podule_t *podule, int timeslice_us)
{
	e200_t *e200 = podule->p;

	timestamp++;

	ne2000_poll(e200->ne2000);

	return 1000;
}

static podule_config_t e200_podule_config =
{
	.items =
	{
		{
			.name = "network_device",
			.description = "Network",
			.type = CONFIG_SELECTION_STRING,
			.selection = 0,
			.default_string = NETWORK_DEVICE_DEFAULT,
		},
		{
			.type = -1
		}
	}
};

static const podule_header_t e200_podule_header =
{
	.version = PODULE_API_VERSION,
	.flags = PODULE_FLAGS_UNIQUE | PODULE_FLAGS_NET,
	.short_name = "e200",
	.name = "Design IT E200 Access+",
	.functions =
	{
		.init = e200_init,
		.close = e200_close,
		.reset = e200_reset,
		.read_b = e200_read_b,
		.read_w = e200_read_w,
		.write_b = e200_write_b,
		.write_w = e200_write_w,
		.run = e200_run
	},
	.config = &e200_podule_config
};

const podule_header_t *podule_probe(const podule_callbacks_t *callbacks, char *path)
{
	e200_log("podule_probe %p path=%s\n", &e200_podule_header, path);

	podule_callbacks = callbacks;
	strcpy(podule_path, path);

	e200_podule_config.items[0].selection = net_get_networks();

	return &e200_podule_header;
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
