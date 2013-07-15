#include "arc.h"
#include "82c711.h"
#include "82c711_fdc.h"

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
           writeide(addr,val);

        if ((addr & ~7) == 0x3f0 && addr != 0x3f6)
           c82c711_fdc_write(addr & 7, val);
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
           return readide(addr);

        if ((addr & ~7) == 0x3f0)
           return c82c711_fdc_read(addr & 7);

        return 0xff;
}
