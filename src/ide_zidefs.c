/*Arculator 2.0 by Sarah Walker*/

/*ZIDEFS compatible IDE podule

  ZIDEFS claims to be compatible with 'ICS compatible' IDE interfaces, but it
  doesn't match the currently emulated ideA podule.

  IOC Address map :
  0000-1fff : ROM
  2000 : ROM page register
  3000-301f : IDE registers
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arc.h"
#include "config.h"
#include "ide_zidefs.h"
#include "ide.h"
#include "podules.h"
#include "podule_api.h"
#include "ide_config.h"

static const podule_callbacks_t *podule_callbacks;

typedef struct zidefs_ide_t
{
        int page;
        uint8_t rom[8192];

        ide_t ide;
} zidefs_ide_t;

static void zidefs_ide_irq_raise();
static void zidefs_ide_irq_clear();

static int zidefs_ide_init(struct podule_t *podule)
{
        FILE *f;
        char fn[512];
        char hd4_fn[512] = {0}, hd5_fn[512] = {0};
        int hd_spt[2], hd_hpc[2], hd_cyl[2];
        const char *p;

        zidefs_ide_t *zidefs = malloc(sizeof(zidefs_ide_t));
        memset(zidefs, 0, sizeof(zidefs_ide_t));

        append_filename(fn, exname, "roms/podules/zidefs/zidefsrom", 511);
        f = fopen(fn, "rb");
        if (f)
        {
                fread(zidefs->rom, 0x2000, 1, f);
                fclose(f);
        }
        else
        {
                rpclog("zidefs_ide_init failed\n");
                free(zidefs);
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

        resetide(&zidefs->ide,
                 hd4_fn, hd_spt[0], hd_hpc[0], hd_cyl[0],
                 hd5_fn, hd_spt[1], hd_hpc[1], hd_cyl[1],
                 zidefs_ide_irq_raise, zidefs_ide_irq_clear);

        podule->p = zidefs;

        return 0;
}

static void zidefs_ide_reset(struct podule_t *podule)
{
        zidefs_ide_t *zidefs = podule->p;

        zidefs->page = 0;
}

static void zidefs_ide_close(struct podule_t *podule)
{
        zidefs_ide_t *zidefs = podule->p;

        closeide(&zidefs->ide);
        free(zidefs);
}

static void zidefs_ide_irq_raise(ide_t *ide)
{
}

static void zidefs_ide_irq_clear(ide_t *ide)
{
}

static uint8_t zidefs_ide_read_b(struct podule_t *podule, podule_io_type type, uint32_t addr)
{
        zidefs_ide_t *zidefs = podule->p;
        uint8_t temp = 0xff;

        if (type != PODULE_IO_TYPE_IOC)
                return 0xff; /*Only IOC accesses supported*/

        switch (addr & 0x3800)
        {
                case 0x0000: case 0x0800: case 0x1000: case 0x1800:
                addr = ((addr & 0x1ffc) | (zidefs->page << 13)) >> 2;
//                rpclog("Read IROM %04X %i %04X %02X\n",addr,icspage,temp,icsrom[temp&0x3FFF]);
                temp = zidefs->rom[addr & 0x3fff];
                break;

                case 0x3000:
                temp = readide(&zidefs->ide, ((addr >> 2) & 7) + 0x1f0);
                break;
        }

        //rpclog("Read ICS %07X %02x %07X\n", addr, temp, PC);
        return temp;
}

static uint16_t zidefs_ide_read_w(struct podule_t *podule, podule_io_type type, uint32_t addr)
{
        zidefs_ide_t *zidefs = podule->p;
//rpclog("read_w %04x\n", addr);
        if (type != PODULE_IO_TYPE_IOC)
                return 0xffff; /*Only IOC accesses supported*/

        switch (addr & 0x3800)
        {
                case 0x3000:
                return readidew(&zidefs->ide);

                default:
                return zidefs_ide_read_b(podule, type, addr);
        }
}

static void zidefs_ide_write_b(struct podule_t *podule, podule_io_type type, uint32_t addr, uint8_t val)
{
        zidefs_ide_t *zidefs = podule->p;

        if (type != PODULE_IO_TYPE_IOC)
                return; /*Only IOC accesses supported*/

        //rpclog("Write ICS %07X %02X %07X\n",addr,val,PC);
        switch (addr & 0x3c00)
        {
                case 0x2000:
                zidefs->page = val;
                break;

                case 0x3000:
                writeide(&zidefs->ide, ((addr >> 2) & 7) + 0x1f0, val);
                break;
        }
}

static void zidefs_ide_write_w(struct podule_t *podule, podule_io_type type, uint32_t addr, uint16_t val)
{
        zidefs_ide_t *zidefs = podule->p;

        if (type != PODULE_IO_TYPE_IOC)
                return; /*Only IOC accesses supported*/

//        rpclog("write_w %04x %04x\n", addr, val);
        switch (addr & 0x3800)
        {
                case 0x3000:
                writeidew(&zidefs->ide, val);
                break;

                default:
                zidefs_ide_write_b(podule, type, addr, val & 0xff);
                break;
        }
}

static const podule_header_t zidefs_ide_podule_header =
{
        .version = PODULE_API_VERSION,
        .flags = PODULE_FLAGS_UNIQUE,
        .short_name = "zidefs_ide",
        .name = "ZIDEFS IDE Controller",
        .functions =
        {
                .init = zidefs_ide_init,
                .close = zidefs_ide_close,
                .reset = zidefs_ide_reset,
                .read_b = zidefs_ide_read_b,
                .read_w = zidefs_ide_read_w,
                .write_b = zidefs_ide_write_b,
                .write_w = zidefs_ide_write_w
        },
        .config = &ide_podule_config
};

const podule_header_t *zidefs_ide_probe(const podule_callbacks_t *callbacks, char *path)
{
        FILE *f;
        char fn[512];

        podule_callbacks = callbacks;
        ide_config_init(callbacks);

        append_filename(fn, exname, "roms/podules/zidefs/zidefsrom", 511);
        f = fopen(fn, "rb");
        if (!f)
                return NULL;
        fclose(f);
        return &zidefs_ide_podule_header;
}
