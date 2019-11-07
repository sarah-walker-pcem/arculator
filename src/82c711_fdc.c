/*Arculator 2.0 by Sarah Walker
  82c711 FDC emulation*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "arc.h"

#include "82c711_fdc.h"
#include "config.h"
#include "disc.h"
#include "ioc.h"
#include "timer.h"

int discint = -1;

void c82c711_fdc_callback(void *p);
void c82c711_fdc_data(uint8_t dat);
void c82c711_fdc_spindown();
void c82c711_fdc_finishread();
void c82c711_fdc_notfound();
void c82c711_fdc_datacrcerror();
void c82c711_fdc_headercrcerror();
void c82c711_fdc_writeprotect();
int  c82c711_fdc_getdata(int last);
void c82c711_fdc_sectorid(uint8_t track, uint8_t side, uint8_t sector, uint8_t size, uint8_t crc1, uint8_t crc2);
void c82c711_fdc_indexpulse();

/*FDC*/
typedef struct FDC
{
        uint8_t dor,stat,command,dat,st0;
        int head,track[2],sector,drive,lastdrive;
        int pos;
        uint8_t params[16];
        uint8_t res[16];
        int pnum,ptot;
        int rate, density;
        uint8_t specify[2];
        int eot[2];
        int lock;
        int perp;
        uint8_t config, pretrk;
        uint8_t dma_dat;
        int written;
        int tc;
        uint8_t format_dat[256];
        int format_state;
        
        int data_ready;
        int inread;
} FDC;

static FDC fdc;

int lastbyte=0;

void c82c711_fdc_reset(void)
{
        disc_stop(0);
        disc_stop(1);
        disc_set_density(0);
                
        fdc.stat=0x80;
        fdc.pnum=fdc.ptot=0;
        fdc.st0=0xC0;
        fdc.lock = 0;
        fdc.head = 0;

        fdc.inread = 0;
        rpclog("Reset 82c711\n");
}

void c82c711_fdc_init(void)
{
        if (fdctype == FDC_82C711)
        {
                c82c711_fdc_reset();
                
                rpclog("82c711 present\n");
                timer_add(&fdc_timer, c82c711_fdc_callback, NULL, 0);
                fdc_data           = c82c711_fdc_data;
                fdc_spindown       = c82c711_fdc_spindown;
                fdc_finishread     = c82c711_fdc_finishread;
                fdc_notfound       = c82c711_fdc_notfound;
                fdc_datacrcerror   = c82c711_fdc_datacrcerror;
                fdc_headercrcerror = c82c711_fdc_headercrcerror;
                fdc_writeprotect   = c82c711_fdc_writeprotect;
                fdc_getdata        = c82c711_fdc_getdata;
                fdc_sectorid       = c82c711_fdc_sectorid;
                fdc_indexpulse     = c82c711_fdc_indexpulse;
        }
//        motorspin = 45000;
}

void c82c711_fdc_spindown()
{
//        rpclog("82c711 spindown\n");
        motoron = 0;
}

void c82c711_fdc_write(uint16_t addr, uint8_t val)
{
//        rpclog("Write FDC %04X %02X %07X  %02X rate=%i\n", addr, val, PC, fdc.st0, fdc.rate);
        switch (addr&7)
        {
                case 1: return;
                case 2: /*DOR*/
//                printf("DOR was %02X\n",fdc.dor);
                if (val&4)
                {
                        fdc.stat=0x80;
                        fdc.pnum=fdc.ptot=0;
                }
                if ((val&4) && !(fdc.dor&4))
                {
//                        rpclog("Reset through 3f2\n");
                        c82c711_fdc_reset();
                        timer_set_delay_u64(&fdc_timer, 16 * TIMER_USEC);
                        discint=-1;
                }
                motoron = (val & 0xf0) ? 1 : 0;                  
                disc_set_motor(motoron);
                disc_drivesel = val & 3;
                fdc.dor=val;
//                rpclog("DOR now %02X\n",val);
                return;
                case 4:
                if (val & 0x80)
                {
//                        rpclog("Reset through 3f4\n");
                        c82c711_fdc_reset();
                        timer_set_delay_u64(&fdc_timer, 16 * TIMER_USEC);
                        discint=-1;
                }
                return;
                case 5: /*Command register*/
//                if (fdc.inread)
//                        rpclog("c82c711_fdc_write : writing while inread! %02X\n", val);
//                rpclog("Write command reg %i %i\n",fdc.pnum, fdc.ptot);
                if (fdc.pnum==fdc.ptot)
                {
                        fdc.tc = 0;
                        fdc.data_ready = 0;
                        
                        fdc.command=val;
//                        rpclog("Starting FDC command %02X\n",fdc.command);
                        switch (fdc.command&0x1F)
                        {
                                case 3: /*Specify*/
                                fdc.pnum=0;
                                fdc.ptot=2;
                                fdc.stat=0x90;
                                break;
                                case 4: /*Sense drive status*/
                                fdc.pnum=0;
                                fdc.ptot=1;
                                fdc.stat=0x90;
                                break;
                                case 5: /*Write data*/
//                                printf("Write data!\n");
                                fdc.pnum=0;
                                fdc.ptot=8;
                                fdc.stat=0x90;
                                fdc.pos=0;
//                                readflash=1;
                                break;
                                case 6: /*Read data*/
                                fdc.pnum=0;
                                fdc.ptot=8;
                                fdc.stat=0x90;
                                fdc.pos=0;
                                break;
                                case 7: /*Recalibrate*/
                                fdc.pnum=0;
                                fdc.ptot=1;
                                fdc.stat=0x90;
                                break;
                                case 8: /*Sense interrupt status*/
//                                printf("Sense interrupt status %i\n",curdrive);
                                fdc.lastdrive = curdrive;
//                                fdc.stat = 0x10 | (fdc.stat & 0xf);
//                                fdc_time=1024;
                                discint = 8;
                                fdc.pos = 0;
                                c82c711_fdc_callback(NULL);
                                break;
                                case 10: /*Read sector ID*/
                                fdc.pnum=0;
                                fdc.ptot=1;
                                fdc.stat=0x90;
                                fdc.pos=0;
                                break;
                                case 0x0d: /*Format track*/
                                fdc.pnum=0;
                                fdc.ptot=5;
                                fdc.stat=0x90;
                                fdc.pos=0;
                                fdc.format_state = 0;
                                break;
                                case 15: /*Seek*/
                                fdc.pnum=0;
                                fdc.ptot=2;
                                fdc.stat=0x90;
                                break;
                                case 0x0e: /*Dump registers*/
                                fdc.lastdrive = curdrive;
                                discint = 0x0e;
                                fdc.pos = 0;
                                c82c711_fdc_callback(NULL);
                                break;
                                case 0x10: /*Get version*/
                                fdc.lastdrive = curdrive;
                                discint = 0x10;
                                fdc.pos = 0;
                                c82c711_fdc_callback(NULL);
                                break;
                                case 0x12: /*Set perpendicular mode*/
                                fdc.pnum=0;
                                fdc.ptot=1;
                                fdc.stat=0x90;
                                fdc.pos=0;
                                break;
                                case 0x13: /*Configure*/
                                fdc.pnum=0;
                                fdc.ptot=3;
                                fdc.stat=0x90;
                                fdc.pos=0;
                                break;
                                case 0x14: /*Unlock*/
                                case 0x94: /*Lock*/
                                fdc.lastdrive = curdrive;
                                discint = fdc.command;
                                fdc.pos = 0;
                                c82c711_fdc_callback(NULL);
                                break;

                                case 0x18:
                                fdc.stat = 0x10;
                                discint  = 0xfc;
                                c82c711_fdc_callback(NULL);
                                break;

                                default:
#ifndef RELEASE_BUILD
                                fatal("Bad FDC command %02X\n",val);
#endif
                                fdc.stat=0x10;
                                discint=0xfc;
                                timer_set_delay_u64(&fdc_timer, 25 * TIMER_USEC);
                                break;
                        }
                }
                else
                {
                        fdc.params[fdc.pnum++]=val;
                        if (fdc.pnum==fdc.ptot)
                        {
//                                rpclog("Got all params %02X\n", fdc.command);
                                fdc.stat=0x30;
                                discint=fdc.command&0x1F;
                                timer_set_delay_u64(&fdc_timer, 128 * TIMER_USEC);
                                curdrive = fdc.params[0] & 3;
                                switch (discint)
                                {
                                        case 5: /*Write data*/
                                        fdc.track[curdrive]=fdc.params[1];
                                        fdc.head=fdc.params[2];
                                        fdc.sector=fdc.params[3];
                                        fdc.eot[curdrive] = fdc.params[4];
                                        disc_writesector(curdrive, fdc.sector, fdc.track[curdrive], fdc.head, fdc.density);
                                        timer_disable(&fdc_timer);
                                        fdc.written = 0;
                                        readflash[curdrive] = 1;
                                        fdc.pos = 0;
                                        ioc_fiq(IOC_FIQ_DISC_DATA);
                                        break;
                                        
                                        case 6: /*Read data*/
                                        fdc.track[curdrive]=fdc.params[1];
                                        fdc.head=fdc.params[2];
                                        fdc.sector=fdc.params[3];
                                        fdc.eot[curdrive] = fdc.params[4];
                                        disc_readsector(curdrive, fdc.sector, fdc.track[curdrive], fdc.head, fdc.density);
                                        timer_disable(&fdc_timer);
                                        readflash[curdrive] = 1;
                                fdc.inread = 1;
                                        break;
                                        
                                        case 7: /*Recalibrate*/
                                        fdc.stat =  1 << curdrive;
                                        timer_disable(&fdc_timer);
                                        disc_seek(curdrive, 0);
                                        break;

                                        case 0x0d: /*Format*/
                                        fdc.format_state = 1;
                                        fdc.pos = 0;
                                        fdc.stat = 0x30;
                                        break;
                                        
                                        case 0xf: /*Seek*/
                                        fdc.stat =  1 << curdrive;
                                        fdc.head = (fdc.params[0] & 4) ? 1 : 0;
                                        timer_disable(&fdc_timer);
                                        disc_seek(curdrive, fdc.params[1]);
                                        break;
                                        
                                        case 10: /*Read sector ID*/
                                        timer_disable(&fdc_timer);
                                        fdc.head = (fdc.params[0] & 4) ? 1 : 0;                                        
//                                        rpclog("Read sector ID %i %i\n", fdc.rate, curdrive);
                                        disc_readaddress(curdrive, fdc.track[curdrive], fdc.head, fdc.density);
                                        break;
                                }
                        }
                }
                return;
                case 7:
                fdc.rate=val&3;
                switch (val & 3)
                {
                        case 0: fdc.density = 2; break;
                        case 1: case 2: fdc.density = 1; break;
                        case 3: fdc.density = 3; break;
                }
                disc_set_density(fdc.density);
//                rpclog("FDC rate = %i\n", val & 3);
                return;
        }
//        printf("Write FDC %04X %02X\n",addr,val);
//        dumpregs();
//        exit(-1);
}

int paramstogo=0;
uint8_t c82c711_fdc_read(uint16_t addr)
{
        uint8_t temp;
//        /*if (addr!=0x3f4) */rpclog("Read FDC %04X %07X %i %02X ",addr,PC,fdc.pos,fdc.st0);
        switch (addr&7)
        {
                case 1: /*???*/
                temp=0x50;
                break;
                case 3:
                temp = 0x20;
                break;
                case 4: /*Status*/
                ioc_irqbc(IOC_IRQB_DISC_IRQ);
                temp=fdc.stat;
                break;
                case 5: /*Data*/
                fdc.stat&=~0x80;
                if (paramstogo)
                {
                        paramstogo--;
                        temp=fdc.res[10 - paramstogo];
//                        rpclog("Read param %i %02X\n",10-paramstogo,temp);
                        if (!paramstogo)
                        {
                                fdc.stat=0x80;
//                                fdc.st0=0;
                        }
                        else
                        {
                                fdc.stat|=0xC0;
//                                c82c711_fdc_callback(NULL);
                        }
                }
                else
                {
                        if (lastbyte)
                           fdc.stat=0x80;
                        lastbyte=0;
                        temp=fdc.dat;
                        fdc.data_ready = 0;
                }
                if (discint==0xA) 
                {
                        timer_set_delay_u64(&fdc_timer, 128 * TIMER_USEC);
                }
                fdc.stat &= 0xf0;
                break;
                case 7: /*Disk change*/
//                rpclog("FDC read 3f7\n");
                if (fdc.dor & (0x10 << (fdc.dor & 3)))
                   temp = (discchange[fdc.dor & 3] || disc_empty(fdc.dor & 3)) ? 0x80 : 0;
                else
                   temp = 0;
//                printf("- DC %i %02X %02X %i %i - ",fdc.dor & 1, fdc.dor, 0x10 << (fdc.dor & 1), discchanged[fdc.dor & 1], driveempty[fdc.dor & 1]);
//                discchanged[fdc.dor&1]=0;
                break;
                default:
                        temp=0xFF;
//                printf("Bad read FDC %04X\n",addr);
//                dumpregs();
//                exit(-1);
        }
//        /*if (addr!=0x3f4) */rpclog("%02X rate=%i\n",temp,fdc.rate);
        return temp;
}

int fdc_abort_f = 0;

void c82c711_fdc_abort()
{
        fdc_abort_f = 1;
//        pclog("FDC ABORT\n");
}

static int fdc_reset_stat = 0;
void c82c711_fdc_callback(void *p)
{
        int temp;
//        if (fdc.inread)
//                rpclog("c82c711_fdc_callback : while inread! %08X %i %02X  %i\n", discint, curdrive, fdc.st0, ins);
        switch (discint)
        {
                case -3: /*End of command with interrupt*/
//                if (output) printf("EOC - interrupt!\n");
//rpclog("EOC\n");
                ioc_irqb(IOC_IRQB_DISC_IRQ);
                case -2: /*End of command*/
                fdc.stat = (fdc.stat & 0xf) | 0x80;
                return;
                case -1: /*Reset*/
//rpclog("Reset\n");
                ioc_irqb(IOC_IRQB_DISC_IRQ);
                fdc_reset_stat = 4;
                return;
                case 3: /*Specify*/
                fdc.stat=0x80;
                fdc.specify[0] = fdc.params[0];
                fdc.specify[1] = fdc.params[1];
                return;
                case 4: /*Sense drive status*/
                fdc.res[10] = (fdc.params[0] & 7) | 0x28;
                if (!fdc.track[curdrive]) fdc.res[10] |= 0x10;

                fdc.stat = (fdc.stat & 0xf) | 0xd0;
                paramstogo = 1;
                discint = 0;
                return;
                case 5: /*Write data*/
                readflash[curdrive] = 1;
                fdc.sector++;
                if (fdc.sector > fdc.params[5])
                {
                        fdc.sector = 1;
                        if (fdc.command & 0x80)
                        {
                                if (fdc.head)
                                {
                                        fdc.tc = 1;
                                        fdc.track[curdrive]++;
                                }
                                else
                                        fdc.head ^= 1;
                        }
                        else
                        {
                                fdc.tc = 1;
                                fdc.track[curdrive]++;
                        }
                }
                if (fdc.tc)
                {
                        discint=-2;
                        ioc_irqb(IOC_IRQB_DISC_IRQ);
                        fdc.stat=0xD0;
                        fdc.res[4]=(fdc.head?4:0)|curdrive;
                        fdc.res[5]=fdc.res[6]=0;
                        fdc.res[7]=fdc.track[curdrive];
                        fdc.res[8]=fdc.head;
                        fdc.res[9]=fdc.sector;
                        fdc.res[10]=fdc.params[4];
                        paramstogo=7;
                        return;
                }                        
                disc_writesector(curdrive, fdc.sector, fdc.track[curdrive], fdc.head, fdc.density);
                ioc_fiq(IOC_FIQ_DISC_DATA);
                return;
                case 6: /*Read data*/
//                rpclog("Read data %i\n", fdc.tc);
                readflash[curdrive] = 1;
                fdc.sector++;
                if (fdc.sector > fdc.params[5])
                {
                        fdc.sector = 1;
                        if (fdc.command & 0x80)
                        {
                                if (fdc.head)
                                {
                                        fdc.tc = 1;
                                        fdc.track[curdrive]++;
                                }
                                else
                                        fdc.head ^= 1;
                        }
                        else
                        {
                                fdc.tc = 1;
                                fdc.track[curdrive]++;
                        }
                }
                if (fdc.tc)
                {
                        fdc.inread = 0;
                        discint=-2;
                        ioc_irqb(IOC_IRQB_DISC_IRQ);
                        fdc.stat=0xD0;
                        fdc.res[4]=(fdc.head?4:0)|curdrive;
                        fdc.res[5]=fdc.res[6]=0;
                        fdc.res[7]=fdc.track[curdrive];
                        fdc.res[8]=fdc.head;
                        fdc.res[9]=fdc.sector;
                        fdc.res[10]=fdc.params[4];
                        paramstogo=7;
                        return;
                }                        
                disc_readsector(curdrive, fdc.sector, fdc.track[curdrive], fdc.head, fdc.density);
                fdc.inread = 1;
                return;

                case 7: /*Recalibrate*/
                fdc.track[curdrive]=0;
//                if (!driveempty[fdc.dor & 1]) discchanged[fdc.dor & 1] = 0;
                fdc.st0=0x20|curdrive|(fdc.head?4:0);
                discint=-3;
                timer_advance_u64(&fdc_timer, 256 * TIMER_USEC);
//                printf("Recalibrate complete!\n");
                fdc.stat = 0x80 | (1 << curdrive);
                return;

                case 8: /*Sense interrupt status*/
//                pclog("Sense interrupt status %i\n", fdc_reset_stat);
                
                fdc.dat = fdc.st0;

                if (fdc_reset_stat)
                {
                        fdc.st0 = (fdc.st0 & 0xf8) | (4 - fdc_reset_stat) | (fdc.head ? 4 : 0);
                        fdc_reset_stat--;
                }
                fdc.stat    = (fdc.stat & 0xf) | 0xd0;
                fdc.res[9]  = fdc.st0;
                fdc.res[10] = fdc.track[curdrive];
                if (!fdc_reset_stat) fdc.st0 = 0x80;

                paramstogo = 2;
                discint = 0;
                return;
                
                case 0x0d: /*Format track*/
//                rpclog("Format\n");
                if (fdc.format_state == 1)
                {
                        ioc_fiq(IOC_FIQ_DISC_DATA);
                        fdc.format_state = 2;
                        timer_advance_u64(&fdc_timer, 16 * TIMER_USEC);
                }
                else if (fdc.format_state == 2)
                {
                        temp = c82c711_fdc_getdata(fdc.pos == ((fdc.params[2] * 4) - 1));
                        if (temp == -1)
                        {
                                timer_advance_u64(&fdc_timer, 16 * TIMER_USEC);
                                return;
                        }
                        fdc.format_dat[fdc.pos++] = temp;
                        if (fdc.pos == (fdc.params[2] * 4))
                           fdc.format_state = 3;
                        timer_advance_u64(&fdc_timer, 16 * TIMER_USEC);
                }
                else if (fdc.format_state == 4)
                {
//                        rpclog("Format next stage\n");
                        disc_format(curdrive, fdc.track[curdrive], fdc.head, fdc.density);
                        fdc.format_state = 4;
                }
                else
                {
                        discint=-2;
                        ioc_irqb(IOC_IRQB_DISC_IRQ);
                        fdc.stat=0xD0;
                        fdc.res[4] = (fdc.head?4:0)|curdrive;
                        fdc.res[5] = fdc.res[6] = 0;
                        fdc.res[7] = fdc.track[curdrive];
                        fdc.res[8] = fdc.head;
                        fdc.res[9] = fdc.format_dat[fdc.pos - 2] + 1;
                        fdc.res[10] = fdc.params[4];
                        paramstogo=7;
                        fdc.format_state = 0;
                        return;
                }
                return;
                
                case 15: /*Seek*/
                fdc.track[curdrive]=fdc.params[1];
//                if (!driveempty[fdc.dor & 1]) discchanged[fdc.dor & 1] = 0;
//                printf("Seeked to track %i %i\n",fdc.track[curdrive], curdrive);
                fdc.st0=0x20|curdrive|(fdc.head?4:0);
                discint=-3;
                timer_advance_u64(&fdc_timer, 256 * TIMER_USEC);
                fdc.stat = 0x80 | (1 << curdrive);
//                pclog("Stat %02X ST0 %02X\n", fdc.stat, fdc.st0);
                return;
                case 0x0e: /*Dump registers*/
                fdc.stat = (fdc.stat & 0xf) | 0xd0;
                fdc.res[3] = fdc.track[0];
                fdc.res[4] = fdc.track[1];
                fdc.res[5] = 0;
                fdc.res[6] = 0;
                fdc.res[7] = fdc.specify[0];
                fdc.res[8] = fdc.specify[1];
                fdc.res[9] = fdc.eot[curdrive];
                fdc.res[10] = (fdc.perp & 0x7f) | ((fdc.lock) ? 0x80 : 0);
                paramstogo=10;
                discint=0;
                return;

                case 0x10: /*Version*/
                fdc.stat = (fdc.stat & 0xf) | 0xd0;
                fdc.res[10] = 0x90;
                paramstogo=1;
                discint=0;
                return;
                
                case 0x12:
                fdc.perp = fdc.params[0];
                fdc.stat = 0x80;
//                picint(0x40);
                return;
                case 0x13: /*Configure*/
                fdc.config = fdc.params[1];
                fdc.pretrk = fdc.params[2];
                fdc.stat = 0x80;
//                picint(0x40);
                return;
                case 0x14: /*Unlock*/
                fdc.lock = 0;
                fdc.stat = (fdc.stat & 0xf) | 0xd0;
                fdc.res[10] = 0;
                paramstogo=1;
                discint=0;
                return;
                case 0x94: /*Lock*/
                fdc.lock = 1;
                fdc.stat = (fdc.stat & 0xf) | 0xd0;
                fdc.res[10] = 0x10;
                paramstogo=1;
                discint=0;
                return;


                case 0xfc: /*Invalid*/
                fdc.dat = fdc.st0 = 0x80;
//                pclog("Inv!\n");
                //picint(0x40);
                fdc.stat = (fdc.stat & 0xf) | 0xd0;
//                fdc.stat|=0xC0;
                fdc.res[10] = fdc.st0;
                paramstogo=1;
                discint=0;
                return;
        }
//        printf("Bad FDC disc int %i\n",discint);
//        dumpregs();
//        exit(-1);
}

static void fdc_overrun()
{
        disc_stop(curdrive);
        timer_disable(&fdc_timer);

        ioc_irqb(IOC_IRQB_DISC_IRQ);
        fdc.stat=0xD0;
        fdc.res[4]=0x40|(fdc.head?4:0)|curdrive;
        fdc.res[5]=0x10; /*Overrun*/
        fdc.res[6]=0;
        fdc.res[7]=fdc.track[curdrive];
        fdc.res[8]=fdc.head;
        fdc.res[9]=fdc.sector;
        fdc.res[10]=fdc.params[4];
        paramstogo=7;
}

void c82c711_fdc_data(uint8_t dat)
{
        if (fdc.tc)
           return;
//        rpclog("Data time : %i cycles  %i\n", lastcycles - cycles, cycles);
//        lastcycles = cycles;
        if (fdc.data_ready)
        {
//                rpclog("Overrun\n");
                fdc_overrun();
        }
        else
        {
//        rpclog("fdc_data %02X %i\n", dat, fdc_time);
                fdc.dma_dat = dat;
                fdc.data_ready = 1;
                ioc_fiq(IOC_FIQ_DISC_DATA);
        }
}


void c82c711_fdc_finishread()
{
        fdc.inread = 0;
        timer_set_delay_u64(&fdc_timer, 25 * TIMER_USEC);
//        rpclog("fdc_finishread\n");
}

void c82c711_fdc_notfound()
{
        timer_disable(&fdc_timer);

        ioc_irqb(IOC_IRQB_DISC_IRQ);
        fdc.stat=0xD0;
        fdc.res[4]=0x40|(fdc.head?4:0)|curdrive;
        fdc.res[5]=5;
        fdc.res[6]=0;
        fdc.res[7]=0;
        fdc.res[8]=0;
        fdc.res[9]=0;
        fdc.res[10]=0;
        paramstogo=7;
//        rpclog("c82c711_fdc_notfound\n");
}

void c82c711_fdc_datacrcerror()
{
        timer_disable(&fdc_timer);

        ioc_irqb(IOC_IRQB_DISC_IRQ);
        fdc.stat=0xD0;
        fdc.res[4]=0x40|(fdc.head?4:0)|curdrive;
        fdc.res[5]=0x20; /*Data error*/
        fdc.res[6]=0x20; /*Data error in data field*/
        fdc.res[7]=fdc.track[curdrive];
        fdc.res[8]=fdc.head;
        fdc.res[9]=fdc.sector;
        fdc.res[10]=fdc.params[4];
        paramstogo=7;
//        rpclog("c82c711_fdc_datacrcerror\n");
}

void c82c711_fdc_headercrcerror()
{
        timer_disable(&fdc_timer);

        ioc_irqb(IOC_IRQB_DISC_IRQ);
        fdc.stat=0xD0;
        fdc.res[4]=0x40|(fdc.head?4:0)|curdrive;
        fdc.res[5]=0x20; /*Data error*/
        fdc.res[6]=0;
        fdc.res[7]=fdc.track[curdrive];
        fdc.res[8]=fdc.head;
        fdc.res[9]=fdc.sector;
        fdc.res[10]=fdc.params[4];
        paramstogo=7;
//        rpclog("c82c711_fdc_headercrcerror\n");
}

void c82c711_fdc_writeprotect()
{
        timer_disable(&fdc_timer);

        ioc_irqb(IOC_IRQB_DISC_IRQ);
        fdc.stat = 0xD0;
        fdc.res[4] = 0x40 | (fdc.head ? 4 : 0) | curdrive;
        fdc.res[5] = 0x02; /*Not writeable*/
        fdc.res[6] = 0;
        fdc.res[7] = 0;
        fdc.res[8] = 0;
        fdc.res[9] = 0;
        fdc.res[10] = 0;
        paramstogo = 7;
}

int c82c711_fdc_getdata(int last)
{
        uint8_t temp;
//        rpclog("Disc get data\n");
        if (!fdc.written && !fdc.tc) return -1;
        if (!last && !fdc.tc)
        {
                ioc_fiq(IOC_FIQ_DISC_DATA);
        }
        fdc.written = 0;
        temp = fdc.dma_dat;
        fdc.dma_dat = 0;
        return temp;
}

void c82c711_fdc_sectorid(uint8_t track, uint8_t side, uint8_t sector, uint8_t size, uint8_t crc1, uint8_t crc2)
{
//        rpclog("SectorID %i %i %i %i\n", track, side, sector, size);
        ioc_irqb(IOC_IRQB_DISC_IRQ);
        fdc.stat=0xD0;
        fdc.res[4]=(fdc.head?4:0)|curdrive;
        fdc.res[5]=0;
        fdc.res[6]=0;
        fdc.res[7]=track;
        fdc.res[8]=side;
        fdc.res[9]=sector;
        fdc.res[10]=size;
        paramstogo=7;
}

void c82c711_fdc_indexpulse()
{
        ioc_irqa(IOC_IRQA_DISC_INDEX);
//        rpclog("c82c711_fdc_indexpulse\n");
}

//FILE *dmaread_f;
uint8_t c82c711_fdc_dmaread(uint32_t addr)
{
/*        if (!dmaread_f)
                dmaread_f = fopen("dma_read.txt", "wt");
        fprintf(dmaread_f, "dmaread_f : read %02X %08X\n", fdc.dma_dat, addr);*/
//        rpclog("DMA read %08X %02X\n", addr, fdc.dma_dat);
        fdc.data_ready = 0;
        if (addr == 0x302a000) /*Terminal count*/
           fdc.tc = 1;
        ioc_fiqc(IOC_FIQ_DISC_DATA);
        return fdc.dma_dat;
}

void c82c711_fdc_dmawrite(uint32_t addr, uint8_t val)
{
//        rpclog("DMA write %08X %02X\n", addr, val);
        if (addr == 0x302a000) /*Terminal count*/
           fdc.tc = 1;
        fdc.dma_dat = val;
        ioc_fiqc(IOC_FIQ_DISC_DATA);
        fdc.written = 1;
}
