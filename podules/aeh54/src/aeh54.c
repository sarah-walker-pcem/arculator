/*Atomwide/Acorn Ethernet III podule (AEH54)

  IOC address map :
    0000-3fff - ROM (read)
		ROM page (write)

  MEMC address map :
    0000-3fff - SEEQ 8005
    		SEEQ A0 not connected (16-bit config)
    		Arc A6-8 connected to SEEQ A1-3
    		Card width test relies on byte writes duplicating the data in both lanes
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
#include "aeh54.h"
#include "net.h"
#include "seeq8005.h"

#ifdef WIN32
extern __declspec(dllexport) const podule_header_t *podule_probe(const podule_callbacks_t *callbacks, char *path);
#else
#define BOOL int
#define APIENTRY
#endif

const podule_callbacks_t *podule_callbacks;
static char podule_path[512];
static podule_config_t aeh54_podule_config;

typedef struct aeh54_t
{
	uint8_t rom[0x20000];

	uint8_t rom_page;

	seeq8005_t seeq8005;

	podule_t *podule;

	net_t *net;
} aeh54_t;

static FILE *aeh54_logf;
static int timestamp = 0;
void aeh54_log(const char *format, ...)
{
   	char buf[1024];
   	va_list ap;

	return;

	if (!aeh54_logf)
		aeh54_logf = fopen("aeh54_log.txt", "wt");

   	va_start(ap, format);
   	vsprintf(buf, format, ap);
   	va_end(ap);
   	fprintf(aeh54_logf, "[%08i] : ", timestamp);
   	fputs(buf, aeh54_logf);
   	fflush(aeh54_logf);
}
void aeh54_fatal(const char *format, ...)
{
   	char buf[1024];
   	va_list ap;

	return;

	if (!aeh54_logf)
		aeh54_logf = fopen("aeh54_log.txt", "wt");

   	va_start(ap, format);
   	vsprintf(buf, format, ap);
   	va_end(ap);
   	fprintf(aeh54_logf, "[%08i] : ", timestamp);
   	fputs(buf, aeh54_logf);
   	fflush(aeh54_logf);
   	exit(-1);
}

static uint8_t aeh54_read_b(struct podule_t *podule, podule_io_type type, uint32_t addr)
{
	aeh54_t *aeh54 = podule->p;
	uint16_t temp;

	if (type == PODULE_IO_TYPE_IOC)
		return aeh54->rom[((addr >> 2) & 0xfff) + (aeh54->rom_page << 12)];

	temp = seeq8005_read(&aeh54->seeq8005, (addr & 0x1c0) >> 6);
	if (addr & 4)
		return temp >> 8;
	return temp & 0xff;
}

static uint16_t aeh54_read_w(struct podule_t *podule, podule_io_type type, uint32_t addr)
{
	aeh54_t *aeh54 = podule->p;

	if (type == PODULE_IO_TYPE_IOC)
		return aeh54->rom[((addr >> 2) & 0xfff) + (aeh54->rom_page << 12)];

	return seeq8005_read(&aeh54->seeq8005, (addr & 0x1c0) >> 6);
}

static void aeh54_write_b(struct podule_t *podule, podule_io_type type, uint32_t addr, uint8_t val)
{
	aeh54_t *aeh54 = podule->p;

	if (type == PODULE_IO_TYPE_IOC)
		aeh54->rom_page = val & 0x1f;
	else
		seeq8005_write(&aeh54->seeq8005, (addr & 0x1c0) >> 6, val | (val << 8));
}

static void aeh54_write_w(struct podule_t *podule, podule_io_type type, uint32_t addr, uint16_t val)
{
	aeh54_t *aeh54 = podule->p;

	if (type == PODULE_IO_TYPE_IOC)
		aeh54->rom_page = val & 0x1f;
	else
		seeq8005_write(&aeh54->seeq8005, (addr & 0x1c0) >> 6, val);
}


static void aeh54_reset(struct podule_t *podule)
{
	aeh54_t *aeh54 = podule->p;

	aeh54_log("Reset aeh54\n");

	aeh54->rom_page = 0;
}

void aeh54_set_irq(void *p, int irq)
{
	aeh54_t *aeh54 = p;
	//aeh54_log("aeh54_irq: aeh54=%p irq=%i\n", aeh54, irq);
	podule_callbacks->set_irq(aeh54->podule, irq);
}

static int aeh54_init(struct podule_t *podule)
{
	FILE *f;
	char rom_fn[512];
	uint8_t mac[6] = {0x00, 0x02, 0x07, 0x00, 0xda, 0x98};
	aeh54_t *aeh54 = malloc(sizeof(aeh54_t));
	memset(aeh54, 0, sizeof(aeh54_t));

	sprintf(rom_fn, "%sether3.rom", podule_path);
	aeh54_log("aeh54 ROM %s\n", rom_fn);
	f = fopen(rom_fn, "rb");
	if (!f)
	{
		aeh54_log("Failed to open ether3.rom!\n");
		free(aeh54);
		return -1;
	}
	fread(aeh54->rom, 0x20000, 1, f);
	fclose(f);

	sscanf(&aeh54->rom[0x5a], "%02x:%02x:%02x:%02x:%02x:%02x", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);

	const char *network_device = podule_callbacks->config_get_string(podule, "network_device", NETWORK_DEVICE_DEFAULT);
	aeh54->net = net_init(network_device, mac);

	if (!aeh54->net)
	{
		aeh54_log("Failed to open network device\n");
		free(aeh54);
		return -1;
	}

	seeq8005_init(&aeh54->seeq8005, aeh54_set_irq, aeh54, aeh54->net);

	aeh54_log("aeh54_init: podule=%p\n", podule);
	aeh54->podule = podule;
	podule->p = aeh54;
	return 0;
}

static void aeh54_close(struct podule_t *podule)
{
	aeh54_t *aeh54 = podule->p;

	aeh54->net->close(aeh54->net);

	seeq8005_close(&aeh54->seeq8005);
	free(aeh54);
}

static int aeh54_run(struct podule_t *podule, int timeslice_us)
{
	aeh54_t *aeh54 = podule->p;
	static int slirp_time = 0;

	timestamp++;

	seeq8005_poll(&aeh54->seeq8005);

	return 2000;
}

static podule_config_t aeh54_podule_config =
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

static const podule_header_t aeh54_podule_header =
{
	.version = PODULE_API_VERSION,
	.flags = PODULE_FLAGS_UNIQUE,
	.short_name = "aeh54",
	.name = "Acorn Ethernet III podule (AEH54)",
	.functions =
	{
		.init = aeh54_init,
		.close = aeh54_close,
		.reset = aeh54_reset,
		.read_b = aeh54_read_b,
		.read_w = aeh54_read_w,
		.write_b = aeh54_write_b,
		.write_w = aeh54_write_w,
		.run = aeh54_run
	},
	.config = &aeh54_podule_config
};

const podule_header_t *podule_probe(const podule_callbacks_t *callbacks, char *path)
{
	aeh54_log("podule_probe %p path=%s\n", &aeh54_podule_header, path);

	podule_callbacks = callbacks;
	strcpy(podule_path, path);

	aeh54_podule_config.items[0].selection = net_get_networks();

	return &aeh54_podule_header;
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
