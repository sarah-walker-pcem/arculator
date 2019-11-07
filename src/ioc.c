/*Arculator 2.0 by Sarah Walker
  IOC emulation*/
#include <stdio.h>
#include "arc.h"
#include "cmos.h"
#include "disc.h"
#include "ds2401.h"
#include "config.h"
#include "ioc.h"
#include "keyboard.h"
#include "printer.h"
#include "timer.h"

IOC_t ioc;

int ref8m_period;

extern int irq;
int keyway=0;
uint8_t tempkey,iockey,iockey2;
int keydelay=0,keydelay2;

void ioc_updateirqs()
{
        if (fdctype == FDC_WD1770)
        {
                if (discchange[curdrive])
                {
                        ioc.irqb |= IOC_IRQB_DISC_CHANGED;
//                        if (ioc.mskb&0x10) rpclog("Disc change interrupt\n");
                }
                else                      
                        ioc.irqb &= ~IOC_IRQB_DISC_CHANGED;
        }
        if ((ioc.mska&(ioc.irqa|0x80))||(ioc.mskb&ioc.irqb))   irq|=1;
        else                                            irq&=~1;
//        if (irq&1) rpclog("IRQ %02X %02X\n",(ioc.mska&ioc.irqa),(ioc.mskb&ioc.irqb));
}

void dumpiocregs()
{
        printf("IOC regs :\n");
        printf("STAT : %02X %02X %02X  MASK : %02X %02X %02X\n",ioc.irqa,ioc.irqb,ioc.fiq,ioc.mska,ioc.mskb,ioc.mskf);
}

static void load_timer(int timer_nr)
{
        if (ioc.timerl[timer_nr])
                timer_set_delay_u64(&ioc.timers[timer_nr], ((ioc.timerl[timer_nr] + 1) * TIMER_USEC) / 2);
        else
                timer_set_delay_u64(&ioc.timers[timer_nr], ((0x10000 + 1) * TIMER_USEC) / 2);
}

static uint16_t read_timer(int timer_nr)
{
        uint64_t remaining = timer_get_remaining_u64(&ioc.timers[timer_nr]);
        
        remaining = (remaining * 2) / TIMER_USEC;
        
        return remaining & 0xffff;
}

//int genpoll,genpol;
#define gencycdif 0
//#define gencycdif ((genpoll-genpol)<<1)
void ioc_write(uint32_t addr, uint32_t v)
{
//        if ((addr & 0x7c) >= 0x10 && (addr & 0x7c) < 0x40) rpclog("ioc_write %04X %02X %07X\n", addr, v, PC);
        switch (addr & 0x7C)
        {
                case 0x00: 
                i2c_change(v & 2, v & 1);
                if (fdctype == FDC_82C711)
                        ds2401_write(v & 8);
                ioc.ctrl = v & 0xFC; 
                return;
                case 0x04: 
                ioc_irqbc(IOC_IRQB_KEYBOARD_TX);
                keyboard_write(v);
                return;
                case 0x14: 
                ioc.irqa &= ~v; 
                ioc_updateirqs(); 
                return;
                case 0x18: 
                ioc.mska = v; 
                ioc_updateirqs(); 
                return;
                case 0x24: /*????*/
                return; 
                case 0x28: 
                ioc.mskb = v; 
                ioc_updateirqs(); 
                return;
                case 0x34: /*????*/
                return;
                case 0x38: 
                ioc.mskf = v; 
                ioc_fiq(0); 
                return;
                case 0x40: 
                ioc.timerl[0] = (ioc.timerl[0] & 0xff00) | v;
                return;
                case 0x44: 
                ioc.timerl[0] = (ioc.timerl[0] & 0x00ff) | (v << 8);
                return;
                case 0x48:
                load_timer(0);
//                rpclog("T0 count = %08X %i\n", ioc.timerc[0], ioc.timerl[0]);
                return;
                case 0x4C:
                ioc.timerr[0] = read_timer(0);
//                rpclog("T0 read  = %08X %i\n", ioc.timerc[0], ioc.timerr[0]);
                return;
                case 0x50: 
                ioc.timerl[1] = (ioc.timerl[1] & 0xff00) | v;
                return;
                case 0x54: 
                ioc.timerl[1] = (ioc.timerl[1] & 0x00ff) | (v << 8);
                return;
                case 0x58:
                load_timer(1);
                return;
                case 0x5C:
                ioc.timerr[1] = read_timer(1);
                return;
                case 0x60: 
                ioc.timerl[2] = (ioc.timerl[2] & 0xff00) | v; 
                return;
                case 0x64: 
                ioc.timerl[2] = (ioc.timerl[2] & 0x00ff) | (v << 8); 
                return;
                case 0x68: 
                ioc.timerc[2] = ioc.timerl[2]; 
                return;
                case 0x6C: 
                ioc.timerr[2] = ioc.timerc[2]; 
                return;
                case 0x70: 
                ioc.timerl[3] = (ioc.timerl[3] & 0xff00) | v; 
                return;
                case 0x74: 
                ioc.timerl[3] = (ioc.timerl[3] & 0x00ff) | (v << 8); 
                return;
                case 0x78: 
                ioc.timerc[3] = ioc.timerl[3]; 
                return;
                case 0x7C: 
                ioc.timerr[3] = ioc.timerc[3]; 
                return;
        }
#ifndef RELEASE_BUILD
        fatal("Bad IOC write %07X %04X %08X\n",addr,v,PC);
#endif
}

uint8_t ioc_read(uint32_t addr)
{
        uint8_t temp;
//        if ((addr & 0x7c) >= 0x10 && (addr & 0x7c) < 0x40) rpclog("ioc_read %08X %07X\n", addr, PC);
        switch (addr & 0x7C)
        {
                case 0x00: 
                temp = ((i2c_clock) ? 2 : 0) | ((i2c_data) ? 1 : 0) | flyback;
                if (fdctype == FDC_82C711)
                {
                        if ((ioc.ctrl & 8) && ds2401_read())
                                temp |= 8;
                }
                else
                {
                        if (!fdc_ready)
                                temp |= 4;
                        if (printer_get_ack())
                                temp |= 0x40;
                }
                return temp;
                case 0x04: 
                ioc_irqbc(IOC_IRQB_KEYBOARD_RX);
                temp = keyboard_read();
                LOG_KB_MOUSE("keyboard_read %02X\n", temp);
                return temp;
                case 0x10: 
                temp = ioc.irqa; 
                return temp;
                case 0x14: 
                return (ioc.irqa | 0x80) & ioc.mska;
                case 0x18:
                return ioc.mska;
                case 0x20:
                return ioc.irqb;
                case 0x24: 
                return ioc.irqb & ioc.mskb;
                case 0x28: 
                return ioc.mskb;
                case 0x30: 
                return ioc.fiq;
                case 0x34: 
                return ioc.fiq & ioc.mskf;
                case 0x38: 
                return ioc.mskf;
                case 0x40: 
                return ioc.timerr[0];
                case 0x44: 
                return ioc.timerr[0] >> 8;
                case 0x50: 
                return ioc.timerr[1];
                case 0x54: 
                return ioc.timerr[1] >> 8;
                case 0x60: 
                return ioc.timerr[2];
                case 0x64: 
                return ioc.timerr[2] >> 8;
                case 0x70: 
                return ioc.timerr[3];
                case 0x74: 
                return ioc.timerr[3] >> 8;
        }
        return 0;
}

void ioc_fiq(uint8_t v)
{
        ioc.fiq |= v;
        if ((ioc.fiq | 0x80) & ioc.mskf) irq |=  2;
        else                             irq &= ~2;
//        rpclog("FIQ set %02X %02X %i\n", ioc.fiq, ioc.mskf, irq);
}

void ioc_fiqc(uint8_t v)
{
        ioc.fiq &= ~v;
        if ((ioc.fiq | 0x80) & ioc.mskf) irq |=  2;
        else                             irq &= ~2;
//        rpclog("FIQ clear %02X %02X %i\n", ioc.fiq, ioc.mskf, irq);
}

void ioc_irqa(uint8_t v)
{
        ioc.irqa |= v;
        ioc_updateirqs();
}

void ioc_irqac(uint8_t v)
{
        ioc.irqa &= ~v;
        ioc_updateirqs();
}

void ioc_irqb(uint8_t v)
{
        ioc.irqb |= v;
        ioc_updateirqs();
}

void ioc_irqbc(uint8_t v)
{
        ioc.irqb &= ~v;
        ioc_updateirqs();
}

static void ioc_timer_callback(void *p)
{
        int timer_nr = (int)p;
        
        ioc_irqa(timer_nr ? IOC_IRQA_TIMER_1 : IOC_IRQA_TIMER_0);
        
        if (ioc.timerl[timer_nr])
                timer_advance_u64(&ioc.timers[timer_nr], ((ioc.timerl[timer_nr] + 1) * TIMER_USEC) / 2);
        else
                timer_advance_u64(&ioc.timers[timer_nr], ((0x10000 + 1) * TIMER_USEC) / 2);
}

void ioc_reset()
{
        ioc.irqa = ioc.mska = 0;//x10;
        ioc.irqb = ioc.mskb = 0;
        ioc.fiq  = ioc.mskf = 0;
        ioc.irqa=0x10;
        ioc.irqb = 2;
        timer_add(&ioc.timers[0], ioc_timer_callback, (void *)0, 1);
        timer_add(&ioc.timers[1], ioc_timer_callback, (void *)1, 1);
}

void ioc_discchange(int drive)
{
        discchange[drive] = 1;
        ioc_updateirqs();
        rpclog("ioc_discchange on %i - %02X\n", drive, ioc.irqb);
}
void ioc_discchange_clear(int drive)
{
        discchange[drive] = 0;
        ioc_updateirqs();
        rpclog("ioc_discchange_clear on %i - %02X\n", drive, ioc.irqb);
}

void keycallback()
{
        keyboard_write(iockey2);
        ioc_irqb(IOC_IRQB_KEYBOARD_TX);
}

void keycallback2()
{
        if (ioc.irqb&0x80)
        {
                keydelay2+=256;
                return;
        }
        iockey=tempkey;
        ioc_irqb(IOC_IRQB_KEYBOARD_RX);
}
