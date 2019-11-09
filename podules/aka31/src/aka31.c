#ifdef WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include "podule_api.h"
#include "aka31.h"
#include "cdrom.h"
#include "d71071l.h"
#include "scsi_config.h"
#include "sound_out.h"
#include "wd33c93a.h"

#ifdef WIN32
extern __declspec(dllexport) const podule_header_t *podule_probe(const podule_callbacks_t *callbacks, char *path);
#else
#define BOOL int
#define APIENTRY
#endif

const podule_callbacks_t *podule_callbacks;
char podule_path[512];

void aka31_update_ints(podule_t *p);

#define AKA31_POD_IRQ  0x01
#define AKA31_TC_IRQ   0x02
#define AKA31_SBIC_IRQ 0x08

#define AKA31_PAGE_MASK   0x3f
#define AKA31_ENABLE_INTS 0x40
#define AKA31_RESET       0x80

typedef struct aka31_t
{
        uint8_t rom[0x10000];
        uint8_t ram[0x10000];
        
        int page;
        uint8_t intstat;
        
        wd33c93a_t wd;
        d71071l_t dma;
        scsi_bus_t bus;
        
        int wd_poll_time;
        int audio_poll_count;

        void *sound_out;
} aka31_t;

static FILE *aka31_logf;

void aka31_log(const char *format, ...)
{
#ifdef DEBUG_LOG
   	char buf[1024];
   	va_list ap;
   	
	if (!aka31_logf)
		aka31_logf = fopen("aka31_log.txt", "wt");

   	va_start(ap, format);
   	vsprintf(buf, format, ap);
   	va_end(ap);
   	fputs(buf, aka31_logf);
   	fflush(aka31_logf);
#endif
}

void fatal(const char *format, ...)
{
   	char buf[1024];
   	va_list ap;

//	return;

	if (!aka31_logf)
		aka31_logf = fopen("aka31_log.txt", "wt");

   	va_start(ap, format);
   	vsprintf(buf, format, ap);
   	va_end(ap);
   	fputs(buf, aka31_logf);
   	fflush(aka31_logf);
   	exit(-1);
}

void aka31_write_ram(podule_t *podule, uint16_t addr, uint8_t val)
{
        aka31_t *aka31 = podule->p;

        aka31->ram[addr] = val;
}
uint8_t aka31_read_ram(podule_t *podule, uint16_t addr)
{
        aka31_t *aka31 = podule->p;

        return aka31->ram[addr];
}

static uint8_t aka31_ioc_readb(podule_t *podule, uint32_t addr)
{
        aka31_t *aka31 = podule->p;

//        aka31_log("Read aka31 B %04X\n", addr);

        switch (addr & 0x3000)
        {
                case 0x0000: case 0x1000:
                addr = ((addr & 0x1ffc) | ((aka31->page & AKA31_PAGE_MASK) << 13)) >> 2;
                //aka31_log("  ROM %05X %02X\n", addr, aka31->rom[addr]);
                return aka31->rom[addr];

		case 0x2000:
//                aka31_log("Read intstat %02x\n", aka31->intstat);
                return aka31->intstat;

                default:
//                aka31_log("Read aka31 %04X\n", addr);
                return 0xff;
        }

        return 0xff;
}

static uint8_t aka31_memc_readb(podule_t *podule, uint32_t addr)
{
        aka31_t *aka31 = podule->p;

        int temp;

//        aka31_log("Read aka31 MEMC B %04X\n", addr);

        if (!(addr & 0x2000))
        {
                temp = ((addr & 0x1ffe) | ((aka31->page & AKA31_PAGE_MASK) << 13)) >> 1;
//                aka31_log("Read aka31 MEMC B %04X %04x %02x\n", addr, temp, aka31->ram[temp | (addr & 1)]);
                return aka31->ram[temp | (addr & 1)];
        }

	if (aka31->page & AKA31_RESET)
		return 0xff;

        switch (addr & 0x3000)
        {
                case 0x2000:
                return wd33c93a_read(&aka31->wd, addr);

                case 0x3000:
                return d71071l_read(&aka31->dma, addr);

                default:
//                aka31_log("Read aka31 %04X\n", addr);
                return 0xff;
        }

        return 0xff;
}


static uint16_t aka31_memc_readw(podule_t *podule, uint32_t addr)
{
        aka31_t *aka31 = podule->p;

        if (!(addr & 0x2000))
        {
//                uint32_t old_addr = addr;
                addr = ((addr & 0x1ffe) | ((aka31->page & AKA31_PAGE_MASK) << 13)) >> 1;
//                aka31_log("Read aka31 MEMC W %04X %04x %04x\n", old_addr, addr, aka31->ram[addr] | (aka31->ram[addr+1] << 8));
                return aka31->ram[addr] | (aka31->ram[addr+1] << 8);
        }
        return aka31_memc_readb(podule, addr);
}


static void aka31_ioc_writeb(podule_t *podule, uint32_t addr, uint8_t val)
{
        aka31_t *aka31 = podule->p;

//        aka31_log("Write aka31 B %04X %02X\n", addr, val);
        switch (addr & 0x3000)
        {
                case 0x2000:
//                aka31_log("Write intclear\n");
                aka31->intstat &= ~AKA31_TC_IRQ;
                if (!(aka31->intstat & AKA31_SBIC_IRQ))
                {
                        aka31->intstat = 0;
                        podule_callbacks->set_irq(podule, 0);
                }
                break;
                case 0x3000:
//                aka31_log("Write page %02x\n", val);
		if (!(val & AKA31_RESET) && (aka31->page & AKA31_RESET))
			wd33c93a_reset(&aka31->wd);
                aka31->page = val;
		aka31_update_ints(podule);
                return;

//                default:
//                aka31_log("Write aka31 %04X %02X\n", addr, val);
        }
}

static void aka31_memc_writeb(podule_t *podule, uint32_t addr, uint8_t val)
{
        aka31_t *aka31 = podule->p;

//        aka31_log("Write aka31 MEMC B %04X %02X\n", addr, val);

        if (!(addr & 0x2000))
        {
                int temp = ((addr & 0x1ffe) | ((aka31->page & AKA31_PAGE_MASK) << 13)) >> 1;
//                aka31_log(" Write to RAM %04x\n", temp);
                aka31->ram[temp | (addr & 1)] = val;
                return;
        }

	if (aka31->page & AKA31_RESET)
		return;

        switch (addr & 0x3000)
        {
                case 0x2000:
                wd33c93a_write(&aka31->wd, addr, val);
                break;

                case 0x3000:
                d71071l_write(&aka31->dma, addr, val);
                break;

//                default:
//                aka31_log("Write aka31 %04X %02X\n", addr, val);
        }
}


static void aka31_memc_writew(podule_t *podule, uint32_t addr, uint16_t val)
{
        aka31_t *aka31 = podule->p;

//        aka31_log("Write aka31 MEMC W %04X %02X\n", addr, val);
        if (!(addr & 0x2000))
        {
                addr = ((addr & 0x1ffe) | ((aka31->page & AKA31_PAGE_MASK) << 13)) >> 1;
//                aka31_log(" Write to RAM %04x\n", addr);
                aka31->ram[addr] = val & 0xff;
                aka31->ram[addr+1] = (val >> 8);
                return;
        }
        aka31_memc_writeb(podule, addr, val);
}


static uint8_t aka31_read_b(struct podule_t *podule, podule_io_type type, uint32_t addr)
{
        if (type == PODULE_IO_TYPE_IOC)
                return aka31_ioc_readb(podule, addr);
        else if (type == PODULE_IO_TYPE_MEMC)
                return aka31_memc_readb(podule, addr);

        return 0xff;
}

static uint16_t aka31_read_w(struct podule_t *podule, podule_io_type type, uint32_t addr)
{
        if (type == PODULE_IO_TYPE_IOC)
                return aka31_ioc_readb(podule, addr);
        else if (type == PODULE_IO_TYPE_MEMC)
                return aka31_memc_readw(podule, addr);

        return 0xffff;
}

static void aka31_write_b(struct podule_t *podule, podule_io_type type, uint32_t addr, uint8_t val)
{
        if (type == PODULE_IO_TYPE_IOC)
                aka31_ioc_writeb(podule, addr, val);
        else if (type == PODULE_IO_TYPE_MEMC)
                aka31_memc_writeb(podule, addr, val);
}

static void aka31_write_w(struct podule_t *podule, podule_io_type type, uint32_t addr, uint16_t val)
{
        if (type == PODULE_IO_TYPE_IOC)
                aka31_ioc_writeb(podule, addr, val);
        else if (type == PODULE_IO_TYPE_MEMC)
                aka31_memc_writew(podule, addr, val);
}




static void aka31_reset(struct podule_t *podule)
{
        aka31_t *aka31 = podule->p;

        aka31_log("Reset aka31\n");
        aka31->page = 0;
}

static int aka31_init(struct podule_t *podule)
{
        FILE *f;
        char rom_fn[512];
        aka31_t *aka31 = malloc(sizeof(aka31_t));
        memset(aka31, 0, sizeof(aka31_t));

//        append_filename(fn,exname,"zidefs",sizeof(fn));
        sprintf(rom_fn, "%sscsirom", podule_path);
        aka31_log("SCSIROM %s\n", rom_fn);
        f = fopen(rom_fn, "rb");
        if (!f)
        {
                aka31_log("Failed to open SCSIROM!\n");
                return -1;
        }
        fread(aka31->rom, 0x10000, 1, f);
        fclose(f);
        
        aka31->page = 0;
        d71071l_init(&aka31->dma, podule);
        wd33c93a_init(&aka31->wd, podule, &aka31->dma, &aka31->bus);
//        addpodule(NULL,icswritew,icswriteb,NULL,icsreadw,icsreadb,NULL);
        aka31_log("aka31 Initialised!\n");

        aka31->sound_out = sound_out_init(aka31, 44100, 4410, aka31_log, podule_callbacks, podule);
        ioctl_reset();

        podule->p = aka31;
        return 0;
}

static void aka31_close(struct podule_t *podule)
{
        aka31_t *aka31 = podule->p;
        
        sound_out_close(aka31->sound_out);
        wd33c93a_close(&aka31->wd);
        free(aka31);
}

static int aka31_run(struct podule_t *podule, int timeslice_us)
{
        aka31_t *aka31 = podule->p;

//        aka31_log("callback\n");
        aka31->wd_poll_time++;
        if (aka31->wd_poll_time >= 5)
        {
                aka31->wd_poll_time = 0;
                wd33c93a_poll(&aka31->wd);
        }
        wd33c93a_process_scsi(&aka31->wd);
        scsi_bus_timer_run(&aka31->bus, 100);

        aka31->audio_poll_count++;
        if (aka31->audio_poll_count >= 1000)
        {
                int16_t audio_buffer[(44100*2)/10];

                aka31->audio_poll_count = 0;
                memset(audio_buffer, 0, sizeof(audio_buffer));
                ioctl_audio_callback(audio_buffer, (44100*2)/10);
                sound_out_buffer(aka31->sound_out, audio_buffer, 44100/10);
        }

        return 100;
}

void aka31_update_ints(podule_t *podule)
{
        aka31_t *aka31 = podule->p;

	if (aka31->intstat && (aka31->page & AKA31_ENABLE_INTS))
                podule_callbacks->set_irq(podule, 1);
	else
                podule_callbacks->set_irq(podule, 0);
}

void aka31_sbic_int(podule_t *podule)
{
        aka31_t *aka31 = podule->p;

        aka31->intstat |= AKA31_SBIC_IRQ | AKA31_POD_IRQ;
        if (aka31->page & AKA31_ENABLE_INTS)
                podule_callbacks->set_irq(podule, 1);
}
void aka31_sbic_int_clear(podule_t *podule)
{
        aka31_t *aka31 = podule->p;

//        aka31_log("aka31_sbic_int_clear\n");
        aka31->intstat &= ~AKA31_SBIC_IRQ;
        if (!(aka31->intstat & AKA31_TC_IRQ))
        {
                aka31->intstat = 0;
                podule_callbacks->set_irq(podule, 0);
        }
}
void aka31_tc_int(podule_t *podule)
{
        aka31_t *aka31 = podule->p;

        aka31->intstat |= AKA31_TC_IRQ | AKA31_POD_IRQ;
        if (aka31->page & AKA31_ENABLE_INTS)
                podule_callbacks->set_irq(podule, 1);
}

static const podule_header_t aka31_podule_header =
{
        .version = PODULE_API_VERSION,
        .flags = PODULE_FLAGS_UNIQUE,
        .short_name = "aka31",
        .name = "Acorn SCSI podule (AKA31)",
        .functions =
        {
                .init = aka31_init,
                .close = aka31_close,
                .reset = aka31_reset,
                .read_b = aka31_read_b,
                .read_w = aka31_read_w,
                .write_b = aka31_write_b,
                .write_w = aka31_write_w,
                .run = aka31_run
        },
        .config = &aka31_podule_config
};

const podule_header_t *podule_probe(const podule_callbacks_t *callbacks, char *path)
{
        aka31_log("podule_probe %p path=%s\n", &aka31_podule_header, path);
        
        podule_callbacks = callbacks;
        strcpy(podule_path, path);

        scsi_config_init();
        
        return &aka31_podule_header;
}

#ifdef WIN32
BOOL APIENTRY DllMain (HINSTANCE hInst     /* Library instance handle. */ ,
                       DWORD reason        /* Reason this function is being called. */ ,
                       LPVOID reserved     /* Not used. */ )
{
        aka31_log("DllMain\n");
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
