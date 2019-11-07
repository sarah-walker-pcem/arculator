/*Arculator 2.0 by Sarah Walker
  Printer port subsystem*/
#include "arc.h"
#include "config.h"
#include "ioc.h"
#include "joystick.h"
#include "plat_joystick.h"
#include "printer.h"

void printer_set_busy(int busy);
void printer_set_ack(int ack);

/*GamesPad :

  D1 = reset - low edge reloads shift registers
  D2 = clock - low edge clocks shift registers
  BUSY = pad 1 data. Active low
  !ACK = pad 2 data. Active _high_*/
static struct
{
        int reset;
        int clock;
        uint16_t data[2];
        int data_out[2];
} gamespad;

static void gamespad_write(uint8_t val)
{
//        rpclog("gamespad_write: %02x\n", val);
        if (!(val & 2) && gamespad.reset)
        {
                int c;
                
                for (c = 0; c < 2; c++)
                {
                        gamespad.data[c] = 0;
                        
                        if (joystick_state[c].axis[1] < -16383)
                                gamespad.data[c] |= 0x010;
                        if (joystick_state[c].axis[1] > 16383)
                                gamespad.data[c] |= 0x020;
                        if (joystick_state[c].axis[0] < -16383)
                                gamespad.data[c] |= 0x040;
                        if (joystick_state[c].axis[0] > 16383)
                                gamespad.data[c] |= 0x080;

                        if (joystick_state[c].button[0])
                                gamespad.data[c] |= 0x100;
                        if (joystick_state[c].button[1])
                                gamespad.data[c] |= 0x001;
                        if (joystick_state[c].button[2])
                                gamespad.data[c] |= 0x200;
                        if (joystick_state[c].button[3])
                                gamespad.data[c] |= 0x002;
                        if (joystick_state[c].button[4])
                                gamespad.data[c] |= 0x400;
                        if (joystick_state[c].button[5])
                                gamespad.data[c] |= 0x800;
                        if (joystick_state[c].button[6])
                                gamespad.data[c] |= 0x004;
                        if (joystick_state[c].button[7])
                                gamespad.data[c] |= 0x008;
                }
//                rpclog("Latch %04x %04x\n", gamespad.data[0], gamespad.data[1]);
        }
        if (!(val & 4) && gamespad.clock)
        {
                gamespad.data_out[0] = gamespad.data[0] & 1;
                gamespad.data_out[1] = gamespad.data[1] & 1;

                printer_set_busy(!gamespad.data_out[0]);
                printer_set_ack(gamespad.data_out[1]);

//                rpclog("Clock  %04x %i\n", gamespad.data[0], gamespad.data_out[0]);
                gamespad.data[0] >>= 1;
                gamespad.data[1] >>= 1;
        }

        gamespad.reset = (val & 2);
        gamespad.clock = (val & 4);
}

uint8_t gamespad_read(void)
{
        uint8_t temp = 0x3f;
        
        if (!gamespad.data_out[0])
                temp |= 0x80;
        if (gamespad.data_out[1])
                temp |= 0x40;
//        rpclog("gamespad_read: %02x  %08x\n", temp, armregs[3]);
        return temp;
}

static void serial_port_write(uint8_t val)
{
        int busy = 0, ack = 0;

        if (!(val & 0x01))
        {
                if ((joystick_state[0].axis[1] < -16383))
                        busy = 1;
                if ((joystick_state[1].axis[1] < -16383))
                        ack = 1;
        }
        if (!(val & 0x02))
        {
                if ((joystick_state[0].axis[1] > 16383))
                        busy = 1;
                if ((joystick_state[1].axis[1] > 16383))
                        ack = 1;
        }
        if (!(val & 0x04))
        {
                if ((joystick_state[0].axis[0] < -16383))
                        busy = 1;
                if ((joystick_state[1].axis[0] < -16383))
                        ack = 1;
        }
        if (!(val & 0x08))
        {
                if ((joystick_state[0].axis[0] > 16383))
                        busy = 1;
                if ((joystick_state[1].axis[0] > 16383))
                        ack = 1;
        }
        if (!(val & 0x10))
        {
                if ((joystick_state[0].button[0]))
                        busy = 1;
                if ((joystick_state[1].button[0]))
                        ack = 1;
        }
        if (!(val & 0x20))
                busy = ack = 1;
        if (!(val & 0x40))
        {
                if ((joystick_state[0].button[1]))
                        busy = 1;
                if ((joystick_state[1].button[1]))
                        ack = 1;
        }

        printer_set_busy(busy);
        printer_set_ack(!ack);
}

#define PRINTER_BUSY 0x80
#define PRINTER_ACK  0x40
#define PRINTER_IRQ  0x04

static int printer_busy, printer_ack;
static int printer_irq_pending;

void printer_set_busy(int busy)
{
        if (fdctype != FDC_82C711)
        {
                if (busy)
                        ioc_irqa(IOC_IRQA_PRINTER_BUSY);
                else
                        ioc_irqac(IOC_IRQA_PRINTER_BUSY);
        }

        printer_busy = busy;
}
void printer_set_ack(int ack)
{
        if (fdctype != FDC_82C711)
        {
                if (ack && !printer_ack)
                        ioc_irqa(IOC_IRQA_PRINTER_ACK);
                else
                        ioc_irqac(IOC_IRQA_PRINTER_ACK);
        }
        else
        {
                printer_irq_pending = 1;
                if (ack && !printer_ack)
                        ioc_irqa(IOC_IRQA_PRINTER_LATCH);
        }

        printer_ack = ack;
}
int printer_get_ack(void)
{
        return printer_ack;
}

void printer_data_write(uint8_t val)
{
//        rpclog("printer_data_write %02x\n", val);
        if (joystick_gamespad_present)
                gamespad_write(val);
        else if (joystick_serial_port_present)
                serial_port_write(val);
}

uint8_t printer_status_read(void)
{
        uint8_t temp = 0x3f;
        
        if (printer_busy)
                temp |= PRINTER_BUSY;
        if (printer_ack)
                temp |= PRINTER_ACK;
        if (printer_irq_pending)
                temp &= ~PRINTER_IRQ;
        printer_irq_pending = 0;
//        rpclog("Read status %02x\n", temp);
        
        return temp;
}
