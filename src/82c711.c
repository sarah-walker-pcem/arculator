/*Arculator 2.0 by Sarah Walker
  82c711 SuperIO emulation*/
#include "arc.h"
#include "82c711.h"
#include "82c711_fdc.h"
#include "config.h"
#include "ide.h"
#include "ioc.h"
#include "printer.h"

static int configmode, configindex;
static uint8_t configregs[16];

void c82c711_write(uint32_t addr, uint8_t val)
{
//        rpclog("c82c711 write %08X %02X  ", addr, val);
        addr = (addr >> 2) & 0x3ff;
//        rpclog("%03X %03X\n", addr, addr & ~7);

        if (configmode!=2)
        {
                if ((addr == 0x3F0) && (val == 0x55))
                {
                        configmode++;
                        return;
                }
                else
                   configmode = 0;
        }
        if (configmode == 2 && addr == 0x3F0 && val == 0xAA)
        {
                configmode = 0;
//                printf("Cleared config mode\n");
                return;
        }
        if (configmode == 2 && addr == 0x3F0)
        {
                configindex = val;
                return;
        }
        if (configmode == 2 && addr == 0x3F1)
        {
                configregs[configindex & 15] = val;
                return;
        }


        if ((addr & ~7) == 0x1f0 || addr == 0x3F6)
           writeide(&ide_internal, addr, val);

        if ((addr & ~7) == 0x3f0 && addr != 0x3f6)
           c82c711_fdc_write(addr & 7, val);

        if ((addr & 0x3ff) == 0x278)
                printer_data_write(val);
}

uint8_t c82c711_read(uint32_t addr)
{
//        rpclog("c82c711 read %08X  ", addr);
        addr = (addr >> 2) & 0x3ff;
//        rpclog("%03X\n", addr);

        if (configmode == 2 && addr == 0x3F1)
        {
                return configregs[configindex & 15];
        }

        if (addr == 0x391) return 0xE4; /*RISC OS 3.00*/
        if (addr == 0x3FF) return 0x55;

        if ((addr & ~7) == 0x1f0 || addr == 0x3F6)
           return readide(&ide_internal, addr);

        if ((addr & ~7) == 0x3f0)
           return c82c711_fdc_read(addr & 7);

        if ((addr & 0x3ff) == 0x279)
                return printer_status_read();
                
        return 0xff;
}

static void c82c711_ide_irq_raise(ide_t *ide)
{
        ioc_irqb(IOC_IRQB_IDE);
}
static void c82c711_ide_irq_clear(ide_t *ide)
{
        ioc_irqbc(IOC_IRQB_IDE);
}

void c82c711_init(void)
{
        resetide(&ide_internal,
                 hd_fn[0], hd_spt[0], hd_hpc[0], hd_cyl[0],
                 hd_fn[1], hd_spt[1], hd_hpc[1], hd_cyl[1],
                 c82c711_ide_irq_raise, c82c711_ide_irq_clear);
}
