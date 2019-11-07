/*Arculator 2.0 by Sarah Walker
  Support modules podule emulation*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "arc.h"
#include "config.h"
#include "podule_api.h"

typedef struct arcrom_t
{
        int page;
        uint8_t rom[0x10000];
} arcrom_t;

static int arcrom_init(struct podule_t *podule)
{
        FILE *f;
        char fn[512];

        rpclog("arcrom_init\n");
        arcrom_t *arcrom = malloc(sizeof(arcrom_t));
        memset(arcrom, 0, sizeof(arcrom_t));

        append_filename(fn, exname, "roms/podules/arcrom/arcrom", 511);
        f = fopen(fn, "rb");
        if (f)
        {
                fread(arcrom->rom, 0x10000, 1, f);
                fclose(f);
        }
        else
        {
                rpclog("arcrom_init failed\n");
                free(arcrom);
                return -1;
        }

        podule->p = arcrom;
        
        return 0;
}

static void arcrom_close(struct podule_t *podule)
{
        arcrom_t *arcrom = podule->p;
        
        free(arcrom);
}

static void arcrom_reset(struct podule_t *podule)
{
        arcrom_t *arcrom = podule->p;

        arcrom->page = 0;
}

static void arcrom_write_b(struct podule_t *podule, podule_io_type type, uint32_t addr, uint8_t val)
{
        arcrom_t *arcrom = podule->p;

        if (type != PODULE_IO_TYPE_IOC)
                return; /*Only IOC accesses supported*/
                
        rpclog("Write ARC %07X %02X %07X\n",addr,val,PC);
        switch (addr & 0x3000)
        {
                case 0x2000:
                arcrom->page = val;
                break;
        }
}

static uint8_t arcrom_read_b(struct podule_t *podule, podule_io_type type, uint32_t addr)
{
        arcrom_t *arcrom = podule->p;
        uint32_t old_addr = addr;

        if (type != PODULE_IO_TYPE_IOC)
                return 0xff; /*Only IOC accesses supported*/

//        if (romset<2 || romset>3) return 0xFF;
        rpclog("Read ARC %07X %07X\n",addr,PC);
        switch (addr&0x3000)
        {
                case 0x0000: case 0x1000:
                addr = ((addr & 0x1ffc) | (arcrom->page << 13)) >> 2;
                rpclog("Read AROM %04X %i %04X %02X\n", old_addr, arcrom->page, addr, arcrom->rom[addr & 0x7FFF]);
                return arcrom->rom[addr & 0xffff];
        }
        return 0xff;
}

static const podule_header_t arcrom_podule_header =
{
        .version = PODULE_API_VERSION,
        .flags = PODULE_FLAGS_UNIQUE,
        .short_name = "arculator_rom",
        .name = "Arculator support modules",
        .functions =
        {
                .init = arcrom_init,
                .close = arcrom_close,
                .reset = arcrom_reset,
                .read_b = arcrom_read_b,
                .write_b = arcrom_write_b
        }
};

const podule_header_t *arcrom_probe(const podule_callbacks_t *callbacks, char *path)
{
        return &arcrom_podule_header;
}
