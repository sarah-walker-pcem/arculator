/*Arculator 2.0 by Sarah Walker*/

/*Acorn AKD52 Hard Disc Controller (ST-506)

  IOC Address map :
  0000-1fff : ROM (2k)
  2000-2fff : HD63463 ST506 controller
  3000-3fff : IRQ enable/status
        bit 0 = IRQ status mirror? Podule ROM suggests bit 3, but ADFS on
                RISC OS 3 overrides the parameters passed to ADFS_HDC and reads
                this bit instead
        bit 3 = IRQ status (read)
        bit 7 = IRQ enable (write)
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arc.h"
#include "arm.h"
#include "config.h"
#include "podules.h"
#include "podule_api.h"
#include "st506.h"
#include "st506_akd52.h"
#include "ide_config.h"

static const podule_callbacks_t *podule_callbacks;

typedef struct akd52_t
{
        uint8_t rom[2048];
        uint8_t irq_status, irq_enable;

        st506_t st506;
} akd52_t;

static void akd52_irq_raise();
static void akd52_irq_clear();

static int akd52_init(struct podule_t *podule)
{
        FILE *f;
        char fn[512];
        char hd4_fn[512] = {0}, hd5_fn[512] = {0};
        int spt[2], hpc[2];
        const char *p;

        akd52_t *akd52 = malloc(sizeof(akd52_t));
        memset(akd52, 0, sizeof(akd52_t));

        append_filename(fn, exname, "roms/podules/akd52/akd52", 511);
        f = fopen(fn, "rb");
        if (f)
        {
                fread(akd52->rom, 0x800, 1, f);
                fclose(f);
        }
        else
        {
                rpclog("akd52_init failed\n");
                free(akd52);
                return -1;
        }

        p = podule_callbacks->config_get_string(podule, "hd4_fn", "");
        if (p)
                strcpy(hd4_fn, p);
        spt[0] = podule_callbacks->config_get_int(podule, "hd4_sectors", 17);
        hpc[0] = podule_callbacks->config_get_int(podule, "hd4_heads", 8);
        p = podule_callbacks->config_get_string(podule, "hd5_fn", "");
        if (p)
                strcpy(hd5_fn, p);
        spt[1] = podule_callbacks->config_get_int(podule, "hd5_sectors", 17);
        hpc[1] = podule_callbacks->config_get_int(podule, "hd5_heads", 8);

        st506_init(&akd52->st506, hd4_fn, spt[0], hpc[0], hd5_fn, spt[1], hpc[1], akd52_irq_raise, akd52_irq_clear, podule);

        podule->p = akd52;

        return 0;
}

static void akd52_close(struct podule_t *podule)
{
        akd52_t *akd52 = podule->p;

        st506_close(&akd52->st506);
        free(akd52);
}

static void akd52_update_irq_status(akd52_t *akd52)
{
        podule_t *podule = akd52->st506.p;

        if (akd52->irq_status && akd52->irq_enable)
                podule_callbacks->set_irq(podule, 1);
        else
                podule_callbacks->set_irq(podule, 0);
}

static void akd52_irq_raise(st506_t *st506)
{
        akd52_t *akd52 = container_of(st506, akd52_t, st506);

        akd52->irq_status = 0x0f;
        akd52_update_irq_status(akd52);
}

static void akd52_irq_clear(st506_t *st506)
{
        akd52_t *akd52 = container_of(st506, akd52_t, st506);

        akd52->irq_status &= ~0x0f;
        akd52_update_irq_status(akd52);
}

static uint8_t akd52_read_b(struct podule_t *podule, podule_io_type type, uint32_t addr)
{
        akd52_t *akd52 = podule->p;
        uint8_t temp = 0xff;

        if (type != PODULE_IO_TYPE_IOC)
                return 0xff; /*Only IOC accesses supported*/

        switch (addr & 0x3000)
        {
                case 0x0000: case 0x1000:
                temp = akd52->rom[(addr >> 2) & 0x7ff];
                break;

                case 0x2000:
                temp = st506_readb(&akd52->st506, addr);
                break;
                
                case 0x3000:
                temp = akd52->irq_status;
                break;
        }

//        rpclog("Read AKD52 %07X %02x %07X  %08x\n", addr, temp, PC, armregs[2]);
        return temp;
}

static uint16_t akd52_read_w(struct podule_t *podule, podule_io_type type, uint32_t addr)
{
        akd52_t *akd52 = podule->p;
//rpclog("read_w %04x\n", addr);
        if (type != PODULE_IO_TYPE_IOC)
                return 0xffff; /*Only IOC accesses supported*/

        switch (addr & 0x3000)
        {
                case 0x2000:
                return st506_readl(&akd52->st506, addr);

                default:
                return akd52_read_b(podule, type, addr);
        }
}

static void akd52_write_b(struct podule_t *podule, podule_io_type type, uint32_t addr, uint8_t val)
{
        akd52_t *akd52 = podule->p;

        if (type != PODULE_IO_TYPE_IOC)
                return; /*Only IOC accesses supported*/

//        rpclog("Write AKD52 %07X %02X %07X\n",addr,val,PC);
        switch (addr & 0x3000)
        {
                case 0x2000:
                st506_writeb(&akd52->st506, addr, val);
                break;

                case 0x3000:
                akd52->irq_enable = val & 0x80;
                akd52_update_irq_status(akd52);
                break;
        }
}

static void akd52_write_w(struct podule_t *podule, podule_io_type type, uint32_t addr, uint16_t val)
{
        akd52_t *akd52 = podule->p;

        if (type != PODULE_IO_TYPE_IOC)
                return; /*Only IOC accesses supported*/

//        rpclog("write_w %04x %04x\n", addr, val);
        switch (addr & 0x3000)
        {
                case 0x2000:
                st506_writel(&akd52->st506, addr, val);
                break;

                default:
                akd52_write_b(podule, type, addr, val & 0xff);
                break;
        }
}

static const podule_header_t akd52_podule_header =
{
        .version = PODULE_API_VERSION,
        .flags = PODULE_FLAGS_UNIQUE,
        .short_name = "akd52",
        .name = "Acorn AKD52 Hard Disc Podule",
        .functions =
        {
                .init = akd52_init,
                .close = akd52_close,
                .read_b = akd52_read_b,
                .read_w = akd52_read_w,
                .write_b = akd52_write_b,
                .write_w = akd52_write_w
        },
        .config = &st506_podule_config
};

const podule_header_t *akd52_probe(const podule_callbacks_t *callbacks, char *path)
{
        FILE *f;
        char fn[512];

        podule_callbacks = callbacks;
        st506_config_init(callbacks);

        append_filename(fn, exname, "roms/podules/akd52/akd52", 511);
        f = fopen(fn, "rb");
        if (!f)
                return NULL;
        fclose(f);
        return &akd52_podule_header;
}
