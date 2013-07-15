/*Arculator 0.8 by Tom Walker
  IOC emulation*/
#include <allegro.h>
#include <stdio.h>
#include "arc.h"
#include "disc.h"
#include "ioc.h"

int irq;
int flyback,fdcready;
int i2cclock,i2cdata;
int keyway=0;
uint8_t tempkey,iockey,iockey2;
int keydelay=0,keydelay2;

void ioc_updateirqs()
{
        if (!fdctype || romset<2)
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

//int genpoll,genpol;
#define gencycdif 0
//#define gencycdif ((genpoll-genpol)<<1)
void ioc_write(uint32_t addr, uint32_t v)
{
//        if ((addr & 0x7c) >= 0x10 && (addr & 0x7c) < 0x40) rpclog("ioc_write %04X %02X %07X\n", addr, v, PC);
        switch (addr & 0x7C)
        {
                case 0x00: 
                cmosi2cchange(v & 2, v & 1); 
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
                ioc.timerc[0] = (ioc.timerl[0] + 2) * ((speed_mhz << 10) >> 1);
//                rpclog("T0 count = %08X %i\n", ioc.timerc[0], ioc.timerl[0]);
                return;
                case 0x4C:
                ioc.timerr[0] = ioc.timerc[0] / ((speed_mhz << 10) >> 1);
//                rpclog("T0 read  = %08X %i\n", ioc.timerc[0], ioc.timerr[0]);
                return;
                case 0x50: 
                ioc.timerl[1] = (ioc.timerl[1] & 0xff00) | v;
                return;
                case 0x54: 
                ioc.timerl[1] = (ioc.timerl[1] & 0x00ff) | (v << 8);
                return;
                case 0x58:
                ioc.timerc[1] = (ioc.timerl[1] + 2) * ((speed_mhz << 10) >> 1);
                return;
                case 0x5C:
                ioc.timerr[1] = ioc.timerc[1] / ((speed_mhz << 10) >> 1);
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
        fatal("Bad IOC write %07X %04X %08X\n",addr,v,PC);
}

uint8_t ioc_read(uint32_t addr)
{
        uint8_t temp;
//        if ((addr & 0x7c) >= 0x10 && (addr & 0x7c) < 0x40) rpclog("ioc_read %08X %07X\n", addr, PC);
        switch (addr & 0x7C)
        {
                case 0x00: 
                return ((i2cclock) ? 2 : 0) | ((i2cdata) ? 1 : 0) | flyback | 4;
                case 0x04: 
                ioc_irqbc(IOC_IRQB_KEYBOARD_RX);
                temp = keyboard_read();
                rpclog("keyboard_read %02X\n", temp);
                return temp;
                case 0x10: 
                temp = ioc.irqa; 
                ioc.irqa &= ~0x10;
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

void ioc_updatetimers()
{
        if (ioc.timerc[0] < 0)
        {
                ioc_irqa(IOC_IRQA_TIMER_0);
                while (ioc.timerc[0] < 0)
                {
                        if (ioc.timerl[0])
                                ioc.timerc[0] += ioc.timerl[0] * ((speed_mhz << 10) >> 1);
                        else
                                ioc.timerc[0] += 0x10000 * ((speed_mhz << 10) >> 1);
                }
        }
        if (ioc.timerc[1] < 0)
        {
                ioc_irqa(IOC_IRQA_TIMER_1);
                while (ioc.timerc[1] < 0)
                {
                        if (ioc.timerl[1])
                                ioc.timerc[1] += ioc.timerl[1] * ((speed_mhz << 10) >> 1);
                        else
                                ioc.timerc[1] += 0x10000 * ((speed_mhz << 10) >> 1);
                }
        }
}

void ioc_reset()
{
        ioc.irqa = ioc.mska = 0;//x10;
        ioc.irqb = ioc.mskb = 0;
        ioc.fiq  = ioc.mskf = 0;
//        ioc.irqa=0x10;
        ioc.irqb = 2;
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

void initjoy()
{
        install_joystick(JOY_TYPE_AUTODETECT);
}

void polljoy()
{
        poll_joystick();
}

uint8_t readjoy(int addr)
{
        int c=(addr&4)?1:0;
        uint8_t temp=0x60;
        if (joy[c].stick[0].axis[1].d1) temp|=0x01;
        if (joy[c].stick[0].axis[1].d2) temp|=0x02;
        if (joy[c].stick[0].axis[0].d1) temp|=0x04;
        if (joy[c].stick[0].axis[0].d2) temp|=0x08;
        if (joy[c].button[0].b) temp|=0x10;
        return temp^0x1F;
}
