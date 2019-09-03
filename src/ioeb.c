#include "arc.h"
#include "config.h"
#include "ioc.h"
#include "ioeb.h"
#include "vidc.h"

static const struct
{
        uint8_t id;
        uint8_t hs;
} monitor_id[4] =
{
        {0xe, 0x1}, /*Standard*/
        {0xb, 0x4}, /*Multisync/SVGA*/
        {0xe, 0x0}, /*Colour VGA*/
        {0xf, 0x0}  /*High res mono - not supported by IOEB systems*/
};

static int hs_invert;

uint8_t ioeb_read(uint32_t addr)
{
        int hs;
        
        switch (addr & 0xf8)
        {
                case 0x50: /*Device ID*/
                return 0x05; /*IOEB*/
                
                case 0x70: /*Monitor ID*/
                if (hs_invert)
                        hs = !vidc_get_hs();
                else
                        hs = vidc_get_hs();

                if (hs)
                        return monitor_id[monitor_type].id | monitor_id[monitor_type].hs;
                return monitor_id[monitor_type].id;
                
                case 0x78: /*Joystick (A3010)*/
                return readjoy(addr);
        }
        
        return 0xff;
}

void ioeb_write(uint32_t addr, uint8_t val)
{
        switch (addr & 0xf8)
        {
                case 0x48:
                vidc_setclock(val & 3);
                hs_invert = val & 4;
                break;
        }
}
