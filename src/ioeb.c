/*Arculator 2.0 by Sarah Walker
  IOEB emulation*/
#include <string.h>
#include "arc.h"
#include "config.h"
#include "ioc.h"
#include "ioeb.h"
#include "joystick.h"
#include "plat_joystick.h"
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
static int has_joystick_ports;

static uint8_t ioeb_joystick_read(int addr)
{
        if (joystick_a3010_present && has_joystick_ports)
        {
                int c = (addr & 4) ? 1 : 0;
                uint8_t temp = 0x7f;

                if (joystick_state[c].axis[1] < -16383)
                        temp &= ~0x01;
                if (joystick_state[c].axis[1] > 16383)
                        temp &= ~0x02;
                if (joystick_state[c].axis[0] < -16383)
                        temp &= ~0x04;
                if (joystick_state[c].axis[0] > 16383)
                        temp &= ~0x08;
                if (joystick_state[c].button[0])
                        temp &= ~0x10;

                return temp;
        }
        else if (has_joystick_ports)
                return 0x7f;
        else
                return 0xff;
}

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
                return ioeb_joystick_read(addr);
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

void ioeb_init()
{
        has_joystick_ports = !strcmp(machine, "a3010");
}
