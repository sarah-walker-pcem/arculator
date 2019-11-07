/*Arculator 2.0 by Sarah Walker*/

/*ICS ideA Hard Disc Interface

  IOC Address map :
  0000-1fff : ROM
  0000 : ROM page register
  
  MEMC address map :
  1004 : read from, result discarded. Interrupt clear maybe?
  2000-21ff : IDE registers - bits 6-8 = register index
  2380 : IDE alternate status
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arc.h"
#include "config.h"
#include "ide_idea.h"
#include "ide.h"
#include "podules.h"
#include "podule_api.h"
#include "ide_config.h"

static const podule_callbacks_t *podule_callbacks;

typedef struct idea_ide_t
{
        int page;
        uint8_t rom[32768];

        ide_t ide;
} idea_ide_t;

static void idea_ide_irq_raise();
static void idea_ide_irq_clear();

static int idea_ide_init(struct podule_t *podule)
{
        FILE *f;
        char fn[512];
        char hd4_fn[512] = {0}, hd5_fn[512] = {0};
        int hd_spt[2], hd_hpc[2], hd_cyl[2];
        const char *p;

        idea_ide_t *idea = malloc(sizeof(idea_ide_t));
        memset(idea, 0, sizeof(idea_ide_t));

        append_filename(fn, exname, "roms/podules/idea/idea", 511);
        f = fopen(fn, "rb");
        if (f)
        {
                fread(idea->rom, 0x8000, 1, f);
                fclose(f);
        }
        else
        {
                rpclog("idea_ide_init failed\n");
                free(idea);
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

        resetide(&idea->ide,
                 hd4_fn, hd_spt[0], hd_hpc[0], hd_cyl[0],
                 hd5_fn, hd_spt[1], hd_hpc[1], hd_cyl[1],
                 idea_ide_irq_raise, idea_ide_irq_clear);

        podule->p = idea;

        return 0;
}

static void idea_ide_reset(struct podule_t *podule)
{
        idea_ide_t *idea = podule->p;

        idea->page = 0;
}

static void idea_ide_close(struct podule_t *podule)
{
        idea_ide_t *idea = podule->p;

        closeide(&idea->ide);
        free(idea);
}

static void idea_ide_irq_raise(ide_t *ide)
{
}

static void idea_ide_irq_clear(ide_t *ide)
{
}

static uint8_t idea_ide_read_b(struct podule_t *podule, podule_io_type type, uint32_t addr)
{
        idea_ide_t *idea = podule->p;
        uint8_t temp = 0xff;

        if (type != PODULE_IO_TYPE_IOC)
        {
                switch (addr & 0x3e00)
                {
                        case 0x2000: /*IDE registers*/
                        temp = readide(&idea->ide, ((addr >> 6) & 7) + 0x1f0);
                        break;
                        case 0x2200: /*IDE alternate status*/
                        if ((addr & 0x1c0) == 0x180)
                                temp = readide(&idea->ide, 0x3f6);
                        break;
                }
                
//                rpclog("Read ICS MEMC %07X %02x %07X\n", addr, temp, PC);
                return temp;
        }

        switch (addr & 0x3800)
        {
                case 0x0000: case 0x0800: case 0x1000: case 0x1800:
                addr = ((addr & 0x1ffc) | (idea->page << 13)) >> 2;
//                rpclog("Read IROM %04X %i %04X %02X\n",addr,icspage,temp,icsrom[temp&0x3FFF]);
                temp = idea->rom[addr & 0x7fff];
                break;
        }

//        rpclog("Read ICS %07X %02x %07X\n", addr, temp, PC);
        return temp;
}

static uint16_t idea_ide_read_w(struct podule_t *podule, podule_io_type type, uint32_t addr)
{
        idea_ide_t *idea = podule->p;
//rpclog("read_w %04x\n", addr);
        if (type != PODULE_IO_TYPE_IOC)
        {
                uint16_t temp = 0xffff;
                
                switch (addr & 0x3e00)
                {
                        case 0x2000: /*IDE registers*/
                        temp = readidew(&idea->ide);
                        break;
                }
//                rpclog("Readw ICS MEMC %07X %04X %07X\n", addr, temp, PC);
                return temp; /*Only IOC accesses supported*/
        }

        switch (addr & 0x3800)
        {
                default:
                return idea_ide_read_b(podule, type, addr);
        }
}

static void idea_ide_write_b(struct podule_t *podule, podule_io_type type, uint32_t addr, uint8_t val)
{
        idea_ide_t *idea = podule->p;

        if (type != PODULE_IO_TYPE_IOC)
        {
                switch (addr & 0x3e00)
                {
                        case 0x2000: /*IDE registers*/
                        writeide(&idea->ide, ((addr >> 6) & 7) + 0x1f0, val);
                        break;
                        case 0x2200: /*IDE alternate status*/
                        if ((addr & 0x1c0) == 0x180)
                                writeide(&idea->ide, 0x3f6, val);
                        break;
                }

//                rpclog("Write ICS MEMC %07X %02X %07X\n",addr,val,PC);
                return; /*Only IOC accesses supported*/
        }

//        rpclog("Write ICS %07X %02X %07X\n",addr,val,PC);
        switch (addr & 0x3c00)
        {
                case 0x0000:
                idea->page = val;
                break;
        }
}

static void idea_ide_write_w(struct podule_t *podule, podule_io_type type, uint32_t addr, uint16_t val)
{
        idea_ide_t *idea = podule->p;

        if (type != PODULE_IO_TYPE_IOC)
        {
                switch (addr & 0x3e00)
                {
                        case 0x2000: /*IDE registers*/
                        writeidew(&idea->ide, val);
                        return;
                }
                return; /*Only IOC accesses supported*/
        }

//        rpclog("write_w %04x %04x\n", addr, val);
        switch (addr & 0x3800)
        {
                default:
                idea_ide_write_b(podule, type, addr, val & 0xff);
                break;
        }
}

static const podule_header_t idea_ide_podule_header =
{
        .version = PODULE_API_VERSION,
        .flags = PODULE_FLAGS_UNIQUE,
        .short_name = "idea_ide",
        .name = "ICS ideA Hard Disc Interface",
        .functions =
        {
                .init = idea_ide_init,
                .close = idea_ide_close,
                .reset = idea_ide_reset,
                .read_b = idea_ide_read_b,
                .read_w = idea_ide_read_w,
                .write_b = idea_ide_write_b,
                .write_w = idea_ide_write_w
        },
        .config = &ide_podule_config
};

const podule_header_t *idea_ide_probe(const podule_callbacks_t *callbacks, char *path)
{
        FILE *f;
        char fn[512];

        podule_callbacks = callbacks;
        ide_config_init(callbacks);
        
        append_filename(fn, exname, "roms/podules/idea/idea", 511);
        f = fopen(fn, "rb");
        if (!f)
                return NULL;
        fclose(f);
        return &idea_ide_podule_header;
}
