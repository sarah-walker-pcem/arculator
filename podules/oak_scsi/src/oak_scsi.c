/*
  Oak 16 Bit SCSI Interface

  This information is somewhat preliminary

  IOC address map:
    0000-3fff - ROM

  MEMC address map (known addresses):
    00-1f - NCR5380
    20    - NCR5380 byte data read, with ACK
    40    - Read status in high byte
    	bit 8 - DRQ
    	bit 9 - BUS_MSG | BUS_CD ?
    220   - NCR5830 word data read, with ACK
    240   - Read status in high byte

    160 bit 0 - EEPROM data read
    D0 - EEPROM data read
    A5 - ACK on read/write
    A6 - Status in high byte?
    A8 - EEPROM chip select?
    A9 - Double-byte SCSI data access?
    A10 - EEPROM clock?
    A11 - EEPROM data write?


  Reads from MEMC space also write to ROM banking latch. A9 and above.

  NCR5380 SCSI
  93C06 EEPROM
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
#include "oak_scsi.h"
#include "cdrom.h"
#include "93c06.h"
#include "ncr5380.h"
#include "scsi_config.h"
#include "sound_out.h"
#include "scsi.h"

#ifdef WIN32
extern __declspec(dllexport) const podule_header_t *podule_probe(const podule_callbacks_t *callbacks, char *path);
#else
#define BOOL int
#define APIENTRY
#endif

const podule_callbacks_t *podule_callbacks;
char podule_path[512];

void oak_scsi_update_ints(podule_t *p);

typedef struct oak_scsi_t
{
	uint8_t rom[0x10000];

	int rom_page;

	//wd33c93a_t wd;
//        d71071l_t dma;
	ncr5380_t ncr;
	scsi_bus_t bus;

	int ncr_poll_time;
	int audio_poll_count;

	void *sound_out;

	eeprom_93c06_t eeprom;
	/*struct
	{
		uint16_t buffer[16];

		int clk;
		int state;
		int nr_bits;
		int addr;
		int data_out;
		int write_enable;
		int dirty;
		uint32_t data;
		uint32_t cmd;
	} eeprom;*/
} oak_scsi_t;

#define EEPROM_CS  (1 << 8)
#define EEPROM_CLK (1 << 10)
#define EEPROM_DI  (1 << 11)

static const uint16_t oak_eeprom_default[16] =
{
	0x081f, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xadef
};

static FILE *oak_scsi_logf;

void oak_scsi_log(const char *format, ...)
{
#ifdef DEBUG_LOG
   	char buf[1024];
   	va_list ap;

	if (!oak_scsi_logf)
		oak_scsi_logf = fopen("oak_scsi_log.txt", "wt");

   	va_start(ap, format);
   	vsprintf(buf, format, ap);
   	va_end(ap);
   	fputs(buf, oak_scsi_logf);
   	fflush(oak_scsi_logf);
#endif
}

void fatal(const char *format, ...)
{
   	char buf[1024];
   	va_list ap;

//	return;

	if (!oak_scsi_logf)
		oak_scsi_logf = fopen("oak_scsi_log.txt", "wt");

   	va_start(ap, format);
   	vsprintf(buf, format, ap);
   	va_end(ap);
   	fputs(buf, oak_scsi_logf);
   	fflush(oak_scsi_logf);
   	exit(-1);
}


void scsi_log(const char *format, ...)
{
#ifdef DEBUG_LOG
   	char buf[1024];
   	va_list ap;

	if (!oak_scsi_logf)
		oak_scsi_logf = fopen("oak_scsi_log.txt", "wt");

   	va_start(ap, format);
   	vsprintf(buf, format, ap);
   	va_end(ap);
   	fputs(buf, oak_scsi_logf);
   	fflush(oak_scsi_logf);
#endif
}

void scsi_fatal(const char *format, ...)
{
   	char buf[1024];
   	va_list ap;

	if (!oak_scsi_logf)
		oak_scsi_logf = fopen("oak_scsi_log.txt", "wt");

   	va_start(ap, format);
   	vsprintf(buf, format, ap);
   	va_end(ap);
   	fputs(buf, oak_scsi_logf);
   	fflush(oak_scsi_logf);
   	exit(-1);
}



static uint8_t oak_scsi_ioc_readb(podule_t *podule, uint32_t addr)
{
	oak_scsi_t *oak_scsi = podule->p;
	uint32_t rom_addr = ((addr & 0x3ffc) >> 2) | ((oak_scsi->rom_page << 12) & 0x3fff);

	return oak_scsi->rom[rom_addr];
}

static uint8_t oak_scsi_memc_readb(podule_t *podule, uint32_t addr)
{
	oak_scsi_t *oak_scsi = podule->p;
	uint8_t ret;
	int cs, clk, di;

	oak_scsi->rom_page = (addr >> 9) & 0xf;

	cs = addr & EEPROM_CS;
	clk = addr & EEPROM_CLK;
	di = addr & EEPROM_DI;

	ret = eeprom_93c06_update(&oak_scsi->eeprom, cs, clk, di) ? 1 : 0;//0xff : 0xfe;

	if (!(addr & 0x100))
	{
		ret = ncr5380_read(&oak_scsi->ncr, addr >> 2);
		if (addr & 0x20)
			ncr5380_dack(&oak_scsi->ncr);
	}

//        oak_scsi_log("Read oak_scsi MEMC B %04X  %02x\n", addr, ret);

	return ret;
}


static uint16_t oak_scsi_memc_readw(podule_t *podule, uint32_t addr)
{
	oak_scsi_t *oak_scsi = podule->p;
	uint16_t ret;
	int cs, clk, di;

	oak_scsi->rom_page = (addr >> 9) & 0xf;

	cs = addr & EEPROM_CS;
	clk = addr & EEPROM_CLK;
	di = addr & EEPROM_DI;

	ret = eeprom_93c06_update(&oak_scsi->eeprom, cs, clk, di) ? 1 : 0;//0xff : 0xfe;

	if (!(addr & 0x100))
	{
		if ((addr & 0x200) && !(addr & 0x5c))
		{
			ret = ncr5380_read(&oak_scsi->ncr, addr >> 2);
			if (addr & 0x20)
				ncr5380_dack(&oak_scsi->ncr);
			ret |= ncr5380_read(&oak_scsi->ncr, addr >> 2) << 8;
			if (addr & 0x20)
				ncr5380_dack(&oak_scsi->ncr);
		}
		else
		{
			ret = ncr5380_read(&oak_scsi->ncr, addr >> 2);
			if (addr & 0x20)
				ncr5380_dack(&oak_scsi->ncr);
			if (ncr5380_drq(&oak_scsi->ncr))
				ret |= 0x100;
			if (ncr5380_bsy(&oak_scsi->ncr))
				ret |= 0x200;
		}
	}

//	oak_scsi_log("Read oak_scsi MEMC W %04X  %04x\n", addr, ret);

	return ret;
}


static void oak_scsi_ioc_writeb(podule_t *podule, uint32_t addr, uint8_t val)
{
	oak_scsi_t *oak_scsi = podule->p;

//        oak_scsi_log("Write oak_scsi B %04X %02X\n", addr, val);
}

static void oak_scsi_memc_writeb(podule_t *podule, uint32_t addr, uint8_t val)
{
	oak_scsi_t *oak_scsi = podule->p;

	//oak_scsi_log("Write oak_scsi MEMC B %04X %02X\n", addr, val);

	ncr5380_write(&oak_scsi->ncr, addr >> 2, val);
	if (addr & 0x20)
		ncr5380_dack(&oak_scsi->ncr);
}


static void oak_scsi_memc_writew(podule_t *podule, uint32_t addr, uint16_t val)
{
	oak_scsi_t *oak_scsi = podule->p;

	//oak_scsi_log("Write oak_scsi MEMC W %04X %02X\n", addr, val);

	if ((addr & 0x200) && !(addr & 0x1c))
	{
		ncr5380_write(&oak_scsi->ncr, addr >> 2, val & 0xff);
		if (addr & 0x20)
			ncr5380_dack(&oak_scsi->ncr);
		ncr5380_write(&oak_scsi->ncr, addr >> 2, val >> 8);
		if (addr & 0x20)
			ncr5380_dack(&oak_scsi->ncr);
	}
	else
	{
		ncr5380_write(&oak_scsi->ncr, addr >> 2, val & 0xff);
		if (addr & 0x20)
			ncr5380_dack(&oak_scsi->ncr);
	}
}


static uint8_t oak_scsi_read_b(struct podule_t *podule, podule_io_type type, uint32_t addr)
{
	if (type == PODULE_IO_TYPE_IOC)
		return oak_scsi_ioc_readb(podule, addr);
	else if (type == PODULE_IO_TYPE_MEMC)
		return oak_scsi_memc_readb(podule, addr);

	return 0xff;
}

static uint16_t oak_scsi_read_w(struct podule_t *podule, podule_io_type type, uint32_t addr)
{
	if (type == PODULE_IO_TYPE_IOC)
		return oak_scsi_ioc_readb(podule, addr);
	else if (type == PODULE_IO_TYPE_MEMC)
		return oak_scsi_memc_readw(podule, addr);

	return 0xffff;
}

static void oak_scsi_write_b(struct podule_t *podule, podule_io_type type, uint32_t addr, uint8_t val)
{
	if (type == PODULE_IO_TYPE_IOC)
		oak_scsi_ioc_writeb(podule, addr, val);
	else if (type == PODULE_IO_TYPE_MEMC)
		oak_scsi_memc_writeb(podule, addr, val);
}

static void oak_scsi_write_w(struct podule_t *podule, podule_io_type type, uint32_t addr, uint16_t val)
{
	if (type == PODULE_IO_TYPE_IOC)
		oak_scsi_ioc_writeb(podule, addr, val);
	else if (type == PODULE_IO_TYPE_MEMC)
		oak_scsi_memc_writew(podule, addr, val);
}




static void oak_scsi_reset(struct podule_t *podule)
{
	oak_scsi_t *oak_scsi = podule->p;

	oak_scsi_log("Reset oak_scsi\n");
	oak_scsi->rom_page = 0;
}

static int oak_scsi_init(struct podule_t *podule)
{
	FILE *f;
	char rom_fn[512];
	oak_scsi_t *oak_scsi = malloc(sizeof(oak_scsi_t));
	memset(oak_scsi, 0, sizeof(oak_scsi_t));

	sprintf(rom_fn, "%sOAK 91 SCSI 1V16 - 128.BIN", podule_path);
	oak_scsi_log("SCSIROM %s\n", rom_fn);
	f = fopen(rom_fn, "rb");
	if (!f)
	{
		oak_scsi_log("Failed to open SCSIROM!\n");
		return -1;
	}
	fread(oak_scsi->rom, 0x10000, 1, f);
	fclose(f);

	sprintf(rom_fn, "%soak_scsi.nvr", podule_path);
	f = fopen(rom_fn, "rb");
	if (f)
	{
		fread(oak_scsi->eeprom.buffer, 32, 1, f);
		fclose(f);
	}
	else
		memcpy(oak_scsi->eeprom.buffer, oak_eeprom_default, 32);

	oak_scsi->rom_page = 0;
	ncr5380_init(&oak_scsi->ncr, podule, podule_callbacks, &oak_scsi->bus);
	oak_scsi_log("oak_scsi Initialised!\n");

	oak_scsi->sound_out = sound_out_init(oak_scsi, 44100, 4410, oak_scsi_log, podule_callbacks, podule);
	ioctl_reset();

	podule->p = oak_scsi;
	return 0;
}

static void oak_scsi_write_eeprom(oak_scsi_t *oak_scsi)
{
	char fn[512];
	FILE *f;

	oak_scsi->eeprom.dirty = 0;

	sprintf(fn, "%soak_scsi.nvr", podule_path);
	f = fopen(fn, "wb");
	if (f)
	{
		fwrite(oak_scsi->eeprom.buffer, 32, 1, f);
		fclose(f);
	}
}

static void oak_scsi_close(struct podule_t *podule)
{
	oak_scsi_t *oak_scsi = podule->p;

	oak_scsi_log("oak_scsi_close: dirty=%i\n", oak_scsi->eeprom.dirty);
	if (oak_scsi->eeprom.dirty)
		oak_scsi_write_eeprom(oak_scsi);

	sound_out_close(oak_scsi->sound_out);
	free(oak_scsi);
}

static int oak_scsi_run(struct podule_t *podule, int timeslice_us)
{
	oak_scsi_t *oak_scsi = podule->p;

//        oak_scsi_log("callback\n");
	oak_scsi->ncr_poll_time++;
	if (oak_scsi->ncr_poll_time >= 5)
	{
		oak_scsi->ncr_poll_time = 0;
	}
	scsi_bus_timer_run(&oak_scsi->bus, 100);

	oak_scsi->audio_poll_count++;
	if (oak_scsi->audio_poll_count >= 1000)
	{
		int16_t audio_buffer[(44100*2)/10];

		oak_scsi->audio_poll_count = 0;
		memset(audio_buffer, 0, sizeof(audio_buffer));
		ioctl_audio_callback(audio_buffer, (44100*2)/10);
		sound_out_buffer(oak_scsi->sound_out, audio_buffer, 44100/10);
	}

	if (oak_scsi->eeprom.dirty)
		oak_scsi_write_eeprom(oak_scsi);

	return 100;
}

static const podule_header_t oak_scsi_podule_header =
{
	.version = PODULE_API_VERSION,
	.flags = PODULE_FLAGS_UNIQUE,
	.short_name = "oak_scsi",
	.name = "Oak 16 Bit SCSI Interface",
	.functions =
	{
		.init = oak_scsi_init,
		.close = oak_scsi_close,
		.reset = oak_scsi_reset,
		.read_b = oak_scsi_read_b,
		.read_w = oak_scsi_read_w,
		.write_b = oak_scsi_write_b,
		.write_w = oak_scsi_write_w,
		.run = oak_scsi_run
	},
	.config = &scsi_podule_config
};

const podule_header_t *podule_probe(const podule_callbacks_t *callbacks, char *path)
{
	oak_scsi_log("podule_probe %p path=%s\n", &oak_scsi_podule_header, path);

	podule_callbacks = callbacks;
	strcpy(podule_path, path);

	scsi_config_init(callbacks);

	return &oak_scsi_podule_header;
}

#ifdef WIN32
BOOL APIENTRY DllMain (HINSTANCE hInst     /* Library instance handle. */ ,
		       DWORD reason        /* Reason this function is being called. */ ,
		       LPVOID reserved     /* Not used. */ )
{
	oak_scsi_log("DllMain\n");
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
