/*Arculator 2.0 by Sarah Walker
  1770 FDC emulation*/
#include <stdio.h>
#include <stdlib.h>

#include "arc.h"
#include "config.h"
#include "disc.h"
#include "ioc.h"
#include "timer.h"
#include "wd1770.h"

#define ABS(x) (((x)>0)?(x):-(x))

void wd1770_callback(void *p);
void wd1770_data(uint8_t dat);
void wd1770_spindown();
void wd1770_finishread();
void wd1770_notfound();
void wd1770_datacrcerror();
void wd1770_headercrcerror();
void wd1770_writeprotect();
int  wd1770_getdata(int last);
void wd1770_fdc_indexpulse();

struct
{
        uint8_t command, sector, track, status, data;
        uint8_t ctrl;
        int curside,curtrack;
        int density;
        int written;
        int stepdir;
        int index_count;
        int rnf_detection;
} wd1770;

int byte;

int wd1770_pos = 0;

void wd1770_reset()
{
        wd1770.status = 0;
        motorspin = 0;
        rpclog("Reset 1770\n");
        if (fdctype == FDC_WD1770)
        {
                rpclog("WD1770 present\n");
                timer_add(&fdc_timer, wd1770_callback, NULL, 0);
                fdc_data           = wd1770_data;
                fdc_spindown       = wd1770_spindown;
                fdc_finishread     = wd1770_finishread;
                fdc_notfound       = wd1770_notfound;
                fdc_datacrcerror   = wd1770_datacrcerror;
                fdc_headercrcerror = wd1770_headercrcerror;
                fdc_writeprotect   = wd1770_writeprotect;
                fdc_getdata        = wd1770_getdata;
                fdc_sectorid       = NULL;
                fdc_indexpulse     = wd1770_fdc_indexpulse;
                
                disc_set_density(0);
        }
//        motorspin = 45000;
}

void wd1770_spinup()
{
//        rpclog("WD1770_spinup\n");
        wd1770.status |= 0x80;
/*        motoron = 1;
        motorspin = 0;
        if (!disc_empty(curdrive))
           fdc_ready = 0;*/
//        rpclog("fdc_ready %i on drive %i (%i)\n", fdc_ready, curdrive, disc_empty(curdrive));
}

void wd1770_spindown()
{
        rpclog("WD1770_spindown\n");
        wd1770.status &= ~0x80;
/*        motoron = 0;
        fdc_ready = 4;*/
}

void wd1770_setspindown()
{
/*        if (!disc_empty(curdrive))
           fdc_ready = 0;*/
//        motorspin = 45000;
}

#define track0 (wd1770.curtrack ? 0 : 4)

void wd1770_write(uint16_t addr, uint8_t val)
{
//        rpclog("Write 1770 %04X %02X\n",addr,val);
        switch (addr & 0xc)
        {
                case 0x0:
                if (wd1770.status & 1 && (val >> 4) != 0xD) { rpclog("Command rejected\n"); return; }
//                rpclog("FDC command %02X %i %i %i\n",val,wd1770.curside,wd1770.track,wd1770.sector);
                
//                if (val == 0x8C && wd1770.curside == 1 && wd1770.track == 0 && wd1770.sector == 3)
//                   output = 1;
                
                wd1770.command = val;
                if ((val >> 4) != 0xD)/* && !(val&8)) */wd1770_spinup();
                switch (val >> 4)
                {
                        case 0x0: /*Restore*/
                        wd1770.status = 0x80 | 0x21 | track0;
                        disc_seek(curdrive, 0);
                        break;
                        
                        case 0x1: /*Seek*/
                        wd1770.status = 0x80 | 0x21 | track0;
                        disc_seek(curdrive, wd1770.data);
                        break;
                        
                        case 0x2:
                        case 0x3: /*Step*/
                        wd1770.status = 0x80 | 0x21 | track0;
                        wd1770.curtrack += wd1770.stepdir;
                        if (wd1770.curtrack < 0) wd1770.curtrack = 0;
                        disc_seek(curdrive, wd1770.curtrack);
                        break;

                        case 0x4:
                        case 0x5: /*Step in*/
                        wd1770.status = 0x80 | 0x21 | track0;
                        wd1770.curtrack++;
                        disc_seek(curdrive, wd1770.curtrack);
                        wd1770.stepdir = 1;
                        break;
                        case 0x6:
                        case 0x7: /*Step out*/
                        wd1770.status = 0x80 | 0x21 | track0;
                        wd1770.curtrack--;
                        if (wd1770.curtrack < 0) wd1770.curtrack = 0;
                        disc_seek(curdrive, wd1770.curtrack);
                        wd1770.stepdir = -1;
                        break;

                        case 0x8: /*Read sector*/
                        wd1770.status = 0x80 | 0x1;
                        disc_readsector(curdrive, wd1770.sector, wd1770.track, wd1770.curside, wd1770.density);
                        //printf("Read sector %i %i %i %i %i\n",curdrive,wd1770.sector,wd1770.track,wd1770.curside,wd1770.density);
                        byte = 0;
                        readflash[curdrive] = 1;
                        wd1770_pos = 0;
                        wd1770.index_count = 0;
                        wd1770.rnf_detection = 1;
                        break;
                        case 0xA: /*Write sector*/
                        wd1770.status = 0x80 | 0x1;
                        disc_writesector(curdrive, wd1770.sector, wd1770.track, wd1770.curside, wd1770.density);
                        byte = 0;
                        ioc_fiq(IOC_FIQ_DISC_DATA);
                        wd1770.status |= 2;

//Carlo Concari: wait for first data byte before starting sector write
                        wd1770.written = 0;
                        wd1770.index_count = 0;
                        wd1770.rnf_detection = 1;
                        break;
                        case 0xC: /*Read address*/
                        wd1770.status = 0x80 | 0x1;
                        disc_readaddress(curdrive, wd1770.track, wd1770.curside, wd1770.density);
                        byte = 0;
                        wd1770.index_count = 0;
                        wd1770.rnf_detection = 1;
                        break;
                        case 0xD: /*Force interrupt*/
//                        rpclog("Force interrupt\n");
                        timer_disable(&fdc_timer);
                        wd1770.status = 0x80 | track0;
                        if (val & 8)
                           ioc_fiq(IOC_FIQ_DISC_IRQ);
                        wd1770_setspindown();
                        break;
                        case 0xF: /*Write track*/
                        wd1770.status = 0x80 | 0x1;
                        disc_format(curdrive, wd1770.track, wd1770.curside, wd1770.density);
                        break;
                        
                        default:
//                                rpclog("Bad 1770 command %02X\n",val);
                        timer_disable(&fdc_timer);
                        ioc_fiq(IOC_FIQ_DISC_IRQ);
                        wd1770.status = 0x90;
                        wd1770_spindown();
                        break;
/*                        rpclog("Bad 1770 command %02X\n", val);
                        dumpregs();
                        mem_dump();
                        exit(-1);*/
                }
                break;
                case 0x4:
                wd1770.track = val;
                break;
                case 0x8:
                wd1770.sector = val;
                break;
                case 0xc:
                ioc_fiqc(IOC_FIQ_DISC_DATA);
                wd1770.status &= ~2;
                wd1770.data = val;
                wd1770.written = 1;
                break;
        }
}

uint8_t wd1770_read(uint16_t addr)
{
//        rpclog("Read 1770 %04X %04X\n",addr,PC);
        switch (addr & 0xc)
        {
                case 0x0:
                ioc_fiqc(IOC_FIQ_DISC_IRQ);
//                rpclog("Status %02X\n",wd1770.status);
                return wd1770.status;
                case 0x4:
                return wd1770.track;
                case 0x8:
                return wd1770.sector;
                case 0xc:
                ioc_fiqc(IOC_FIQ_DISC_DATA);
                wd1770.status &= ~2;
//                rpclog("Read data %02X\n",wd1770.data);
                return wd1770.data;
        }
        return 0xFE;
}

#define FDC_LATCHA_DSKCHG_CLEAR (1 << 7)

void wd1770_writelatch_a(uint8_t val)
{
//        rpclog("Write latch A %02X\n", val);
        if (!(val & 1)) disc_drivesel = curdrive = 0;
        if (!(val & 2)) disc_drivesel = curdrive = 1;
        if (!(val & 4)) disc_drivesel = curdrive = 2;
        if (!(val & 8)) disc_drivesel = curdrive = 3;
        ioc_updateirqs();
        wd1770.curside = (val & 0x10) ? 0 : 1;
        motoron = !(val & 0x20);
        disc_set_motor(motoron);
        if (motoron && !disc_empty(curdrive))
           fdc_ready = 0;
        else
           fdc_ready = 4;
        if (!(val & FDC_LATCHA_DSKCHG_CLEAR) && !disc_empty(curdrive) && (romset < ROM_RISCOS_200))
                ioc_discchange_clear(curdrive);
}
void wd1770_writelatch_b(uint8_t val)
{
//        rpclog("Write latch B %02X\n", val);
        wd1770.density = !(val & 2);
}

void wd1770_callback(void *p)
{
//        rpclog("FDC callback %02X\n", wd1770.command);

        switch (wd1770.command >> 4)
        {
                case 0: /*Restore*/
                wd1770.curtrack = wd1770.track = 0;
                wd1770.status = 0x84;
                wd1770_setspindown();
                ioc_fiq(IOC_FIQ_DISC_IRQ);
//                disc_seek(curdrive,0);
                break;
                case 1: /*Seek*/
                wd1770.curtrack = wd1770.track = wd1770.data;
                wd1770.status = 0x80 | track0;
                wd1770_setspindown();
                ioc_fiq(IOC_FIQ_DISC_IRQ);
//                disc_seek(curdrive,wd1770.curtrack);
                break;
                case 3: /*Step*/
                case 5: /*Step in*/
                case 7: /*Step out*/
                wd1770.track = wd1770.curtrack;
                case 2: /*Step*/
                case 4: /*Step in*/
                case 6: /*Step out*/
                wd1770.status = 0x80 | track0;
                wd1770_setspindown();
//                rpclog("call ioc_fiq(IOC_FIQ_DISC_IRQ)\n");
                ioc_fiq(IOC_FIQ_DISC_IRQ);
                break;

                case 8: /*Read sector*/
                wd1770.status = 0x80 | (wd1770.status & 4);
                wd1770_setspindown();
                ioc_fiq(IOC_FIQ_DISC_IRQ);
                break;
                case 0xA: /*Write sector*/
                wd1770.status = 0x80;
                wd1770_setspindown();
                ioc_fiq(IOC_FIQ_DISC_IRQ);
                break;
                case 0xC: /*Read address*/
                wd1770.status = 0x80 | (wd1770.status & 4);
                wd1770_setspindown();
                ioc_fiq(IOC_FIQ_DISC_IRQ);
                wd1770.sector = wd1770.track;
                break;
                case 0xF: /*Write track*/
                wd1770.status = 0x80;
                wd1770_setspindown();
                ioc_fiq(IOC_FIQ_DISC_IRQ);
                break;
        }
        wd1770.rnf_detection = 0;
}

void wd1770_data(uint8_t dat)
{
        wd1770_pos++;
//        rpclog("wd1770_data %02X %i\n", dat, wd1770_pos);
        if (wd1770.status & 2)
        {
                rpclog("Overrun\n");
                wd1770.status = 0x84;
                ioc_fiq(IOC_FIQ_DISC_IRQ);
                disc_stop(curdrive);
                return;
        }
        wd1770.data = dat;
        wd1770.status |= 2;
        ioc_fiq(IOC_FIQ_DISC_DATA);
}

void wd1770_finishread()
{
//        rpclog("fdc_time set by wd1770_finishread\n");
        timer_set_delay_u64(&fdc_timer, 25 * TIMER_USEC);
}

void wd1770_notfound()
{
//        rpclog("Not found\n");
        timer_disable(&fdc_timer);
        ioc_fiq(IOC_FIQ_DISC_IRQ);
        wd1770.status = 0x90;
        wd1770_spindown();
}

void wd1770_datacrcerror()
{
//        rpclog("Data CRC\n");
        timer_disable(&fdc_timer);
        ioc_fiq(IOC_FIQ_DISC_IRQ);
        wd1770.status = 0x88;
        wd1770_spindown();
}

void wd1770_headercrcerror()
{
//        rpclog("Header CRC\n");
        timer_disable(&fdc_timer);
        ioc_fiq(IOC_FIQ_DISC_IRQ);
        wd1770.status = 0x98;
        wd1770_spindown();
}

int wd1770_getdata(int last)
{
//        rpclog("Disc get data\n");
        if (!wd1770.written) return -1;
        if (!last)
        {
                ioc_fiq(IOC_FIQ_DISC_DATA);
                wd1770.status |= 2;
        }
        wd1770.written = 0;
        return wd1770.data;
}

void wd1770_writeprotect()
{
        timer_disable(&fdc_timer);
        ioc_fiq(IOC_FIQ_DISC_IRQ);
        wd1770.status = 0xC0;
        wd1770_spindown();
}

void wd1770_fdc_indexpulse()
{
        wd1770.index_count++;
        if ((wd1770.index_count == 5) && wd1770.rnf_detection)
                wd1770_notfound();
}
