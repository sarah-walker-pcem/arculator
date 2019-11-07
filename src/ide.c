/*Arculator 2.0 by Sarah Walker
  IDE emulation*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arc.h"
#include "config.h"
#include "ide.h"
#include "ioc.h"
#include "timer.h"

/* Bits of 'atastat' */
#define ERR_STAT		0x01
#define DRQ_STAT		0x08 /* Data request */
#define DSC_STAT                0x10
#define SERVICE_STAT            0x10
#define READY_STAT		0x40
#define BUSY_STAT		0x80

/* Bits of 'error' */
#define ABRT_ERR		0x04 /* Command aborted */
#define MCR_ERR			0x08 /* Media change request */

ide_t ide_internal;

static void ide_raise_irq(ide_t *ide)
{
        if (ide->irq_raise)
                ide->irq_raise(ide);
}
static void ide_clear_irq(ide_t *ide)
{
        if (ide->irq_clear)
                ide->irq_clear(ide);
}

void closeide(ide_t *ide)
{
        if (ide->hdfile[0])
                fclose(ide->hdfile[0]);
        if (ide->hdfile[1])
                fclose(ide->hdfile[1]);
}

void resetide(ide_t *ide,
                char *fn_pri, int pri_spt, int pri_hpc, int pri_cyl,
                char *fn_sec, int sec_spt, int sec_hpc, int sec_cyl,
                void (*irq_raise)(ide_t *ide), void (*irq_clear)(ide_t *ide))
{
        int c;

        closeide(ide);
        memset(ide, 0, sizeof(ide_t));

        ide->drive=0;
        ide->atastat=0x40;
        ide->idebufferb = (uint8_t *)ide->idebuffer;
        ide->irq_raise = irq_raise;
        ide->irq_clear = irq_clear;

        for (c = 0; c < 2; c++)
        {
                if (!c)
                        ide->hdfile[c] = fopen(fn_pri, "rb+");
                else
                        ide->hdfile[c] = fopen(fn_sec, "rb+");

                if (ide->hdfile[c])
                {
                        uint8_t log2secsize, sectors, heads, density;
                        
                        fseek(ide->hdfile[c], 0xFC0, SEEK_SET);
                        log2secsize = getc(ide->hdfile[c]);
                        sectors = getc(ide->hdfile[c]);
                        heads = getc(ide->hdfile[c]);
                        density = getc(ide->hdfile[c]);

                        if ((log2secsize != 8 && log2secsize != 9) || !sectors || !heads || sectors > 63 || heads > 16 || density != 0)
                                ide->skip512[c] = 0;
                        else
                                ide->skip512[c] = 1;
                }
//        rpclog("Drive %i - %i %i\n",c,ide->spt[c],ide->hpc[c]);
        }
        ide->def_spt[0] = ide->spt[0] = pri_spt;
        ide->def_hpc[0] = ide->hpc[0] = pri_hpc;
        ide->def_cyl[0] = ide->cyl[0] = pri_cyl;
        ide->def_spt[1] = ide->spt[1] = sec_spt;
        ide->def_hpc[1] = ide->hpc[1] = sec_hpc;
        ide->def_cyl[1] = ide->cyl[1] = sec_cyl;

//        ide->spt=63;
//        ide->hpc=16;
//        ide->spt=16;
//        ide->hpc=14;

        timer_add(&ide->timer, callbackide, ide, 0);
}

void writeidew(ide_t *ide, uint16_t val)
{
//        if (ide->sector==7) rpclog("Write data %08X %04X\n",ide->pos,val);
        ide->idebuffer[ide->pos>>1]=val;
        ide->pos+=2;
        if (ide->pos>=512)
        {
                ide->pos=0;
                ide->atastat=0x80;
                timer_set_delay_u64(&ide->timer, 1000 * TIMER_USEC);
        }
}

void writeide(ide_t *ide, uint32_t addr, uint8_t val)
{
//        if (addr!=0x1F0) rpclog("Write IDE %08X %02X %08X %08X\n",addr,val,PC-8,armregs[12]);
        switch (addr)
        {
                case 0x1F0:
                ide->idebufferb[ide->pos++]=val;
                if (ide->pos>=512)
                {
                        ide->pos=0;
                        ide->atastat=0x80;
                        timer_set_delay_u64(&ide->timer, 1000 * TIMER_USEC);
                }
                return;
                case 0x1F1:
                ide->cylprecomp=val;
                return;
                case 0x1F2:
                ide->secount=val;
                if (!val) ide->secount=256;
//                rpclog("secount %i %02X\n",ide->secount,val);
                return;
                case 0x1F3:
                ide->sector=val;
                return;
                case 0x1F4:
                ide->cylinder=(ide->cylinder&0xFF00)|val;
                return;
                case 0x1F5:
                ide->cylinder=(ide->cylinder&0xFF)|(val<<8);
                return;
                case 0x1F6:
                ide->head=val&0xF;
                ide->drive=(val>>4)&1;
//                rpclog("Write IDE head %02X %i,%i\n",val,ide->head,ide->drive);
                return;
                case 0x1F7: /*Command register*/
  //              rpclog("Starting new command %02X\n",val);
                ide->command=val;
                ide->error=0;
                switch (val)
                {
                        case 0x10: /*Restore*/
                        case 0x70: /*Seek*/
                        ide->atastat=0x40;
                        timer_set_delay_u64(&ide->timer, 1000 * TIMER_USEC);
                        return;
                        case 0x20: /*Read sector*/
                        case 0x21: /*Read sector, no retry*/
/*                        if (ide->secount>1)
                        {
                                error("Read %i sectors from sector %i cylinder %i head %i\n",ide->secount,ide->sector,ide->cylinder,ide->head);
                                exit(-1);
                        }*/
                        rpclog("Read %i sectors from sector %i cylinder %i head %i\n",ide->secount,ide->sector,ide->cylinder,ide->head);
                        ide->atastat=0x80;
                        timer_set_delay_u64(&ide->timer, 1000 * TIMER_USEC);
                        return;
                        case 0x30: /*Write sector*/
                        case 0x31: /*Write sector, no retry*/
/*                        if (ide->secount>1)
                        {
                                error("Write %i sectors to sector %i cylinder %i head %i\n",ide->secount,ide->sector,ide->cylinder,ide->head);
                                exit(-1);
                        }*/
                        rpclog("Write %i sectors to sector %i cylinder %i head %i\n",ide->secount,ide->sector,ide->cylinder,ide->head);
                        ide->atastat=0x08;
                        ide->pos=0;
                        return;
                        case 0x40: /*Read verify*/
                        case 0x41:
//                        rpclog("Read verify %i sectors from sector %i cylinder %i head %i\n",ide->secount,ide->sector,ide->cylinder,ide->head);
                        ide->atastat=0x80;
                        timer_set_delay_u64(&ide->timer, 1000 * TIMER_USEC);
                        return;
                        case 0x50:
//                        rpclog("Format track %i head %i\n",ide->cylinder,ide->head);
                        ide->atastat=0x08;
//                        idecallback=200;
                        ide->pos=0;
                        return;
                        case 0x91: /*Set parameters*/
                        ide->atastat=0x80;
                        timer_set_delay_u64(&ide->timer, 1000 * TIMER_USEC);
                        return;
                        case 0xA1: /*Identify packet device*/
                        case 0xE3: /*Idle*/
                        ide->atastat=0x80;
                        timer_set_delay_u64(&ide->timer, 1000 * TIMER_USEC);
                        return;
                        case 0xEC: /*Identify device*/
                        ide->atastat=0x80;
                        timer_set_delay_u64(&ide->timer, 1000 * TIMER_USEC);
                        return;
                        case 0xE5: /*Standby power check*/
                        ide->atastat=0x80;
                        timer_set_delay_u64(&ide->timer, 1000 * TIMER_USEC);
                        return;

                        default:
                	ide->atastat = READY_STAT | ERR_STAT | DSC_STAT;
                	ide->error = ABRT_ERR;
                        ide_raise_irq(ide);
                        return;
                }
                return;
                case 0x3F6:
                if ((ide->fdisk&4) && !(val&4))
                {
                        timer_set_delay_u64(&ide->timer, 5000 * TIMER_USEC);
                        ide->reset=1;
                        ide->atastat=0x80;
//                        rpclog("IDE Reset\n");
                }
                ide->fdisk=val;
                return;
        }
#ifndef RELEASE_BUILD
        fatal("Bad IDE write %04X %02X\n",addr,val);
#endif
}

uint8_t readide(ide_t *ide, uint32_t addr)
{
        uint8_t temp;
//        if (addr!=0x1F0) rpclog("Read IDE %08X %08X\n",addr,PC-8);
        switch (addr)
        {
                case 0x1F0:
//                rpclog("Read data %08X %02X\n",ide->pos,idebufferb[ide->pos]);
                temp = ide->idebufferb[ide->pos];
                ide->pos++;
                if (ide->pos>=512)
                {
                        ide->pos=0;
                        ide->atastat=0x40;
                }
                return temp;
                case 0x1F1:
//                rpclog("Read IDEerror %02X\n",ide->atastat);
                return ide->error;
                case 0x1F2:
                return ide->secount;
                case 0x1F3:
                return ide->sector;
                case 0x1F4:
                return ide->cylinder&0xFF;
                case 0x1F5:
                return ide->cylinder>>8;
                case 0x1F6:
//                        rpclog("Read IDE Head Drive %02X %02X\n",ide->head|(ide->drive<<4),armregs[1]);
                return ide->head|(ide->drive<<4)|0xA0;
                case 0x1F7:
                ide_clear_irq(ide);
//                rpclog("Read ATAstat %02X %p\n",ide->atastat, ide);
                return ide->atastat;
                case 0x3F6:
//                rpclog("Read ATAstat %02X\n",ide->atastat);
                return ide->atastat;
        }
#ifndef RELEASE_BUILD
        fatal("Bad IDE read %04X\n",addr);
#endif
        return 0xff;
}

uint16_t readidew(ide_t *ide)
{
        uint16_t temp;
//        if (ide->sector==7) rpclog("Read data2 %08X %04X\n",ide->pos,idebuffer[ide->pos>>1]);
        temp = ide->idebuffer[ide->pos>>1];
//        rpclog("Read IDEW %04X %i\n",temp, ide->pos);
        ide->pos+=2;
        if (ide->pos>=512)
        {
                ide->pos=0;
                ide->atastat=0x40;
                if (ide->command == 0x20 || ide->command == 0x21)
                {
                        ide->secount--;
//                        rpclog("Sector done - secount %i\n",ide->secount);
                        if (ide->secount)
                        {
                                ide->atastat=0x08;
                                ide->sector++;
                                if (ide->sector==(ide->spt[ide->drive]+1))
                                {
                                        ide->sector=1;
                                        ide->head++;
                                        if (ide->head==(ide->hpc[ide->drive]))
                                        {
                                                ide->head=0;
                                                ide->cylinder++;
                                        }
                                }
                                ide->atastat=0x80;
                                timer_set_delay_u64(&ide->timer, 1000 * TIMER_USEC);
                        }
                }
        }
        return temp;
}

void resetide_drive(ide_t *ide)
{
        timer_set_delay_u64(&ide->timer, 1000 * TIMER_USEC);
        ide->reset=1;
        ide->atastat=0x80;
        rpclog("Requested reset\n");
}

void callbackide(void *p)
{
        ide_t *ide = p;
        int addr,c;
        rpclog("IDE callback: drive=%i reset=%i command=%02x skip512=%i %p\n", ide->drive, ide->reset, ide->command, ide->skip512[ide->drive], ide);
//        rpclog("IDE callback %08X %i %02X\n",hdfile[ide->drive],ide->drive,ide->command);
	if (!ide->hdfile[ide->drive])
	{
                ide->atastat=0x41;
                ide->error=4;
                ide_raise_irq(ide);
		return;
	}
        if (ide->reset)
        {
                ide->atastat=0x40;
                ide->error=0;
                ide->secount=1;
                ide->sector=1;
                ide->head=0;
                ide->cylinder=0;
                ide->reset=0;
//                rpclog("Reset callback\n");
                return;
        }
        switch (ide->command)
        {
                case 0x10: /*Restore*/
                case 0x70: /*Seek*/
//                rpclog("Restore callback\n");
                ide->atastat=0x40;
                ide_raise_irq(ide);
                return;
                case 0x20: /*Read sectors*/
                case 0x21: /*Read sectors, no retry*/
                if (!ide->secount)
                {
                        ide->atastat=0x40;
                        return;
                }
                readflash[0]=1;
                addr=((((ide->cylinder*ide->hpc[ide->drive])+ide->head)*ide->spt[ide->drive])+(ide->sector))*512;
                if (!ide->skip512[ide->drive]) addr-=512;
                /*                if (ide->cylinder || ide->head)
                {
                        error("Read from other cylinder/head");
                        exit(-1);
                }*/
                rpclog("Seek to %08X\n",addr);
                fseek(ide->hdfile[ide->drive],addr,SEEK_SET);
                fread(ide->idebuffer,512,1,ide->hdfile[ide->drive]);
                ide->pos=0;
                ide->atastat=0x08;
//                rpclog("Read sector callback %i %i %i offset %08X %i left %i\n",ide->sector,ide->cylinder,ide->head,addr,ide->secount,ide->spt[ide->drive]);
                ide_raise_irq(ide);
                return;
                case 0x30: /*Write sector*/
                case 0x31: /*Write sector, no retry*/
                readflash[0]=2;
                addr=((((ide->cylinder*ide->hpc[ide->drive])+ide->head)*ide->spt[ide->drive])+(ide->sector))*512;
                if (!ide->skip512[ide->drive]) addr-=512;
//                rpclog("Write sector callback %i %i %i offset %08X %i left %i %i %i\n",ide->sector,ide->cylinder,ide->head,addr,ide->secount,ide->spt[ide->drive],ide->hpc[ide->drive],ide->drive);
                fseek(ide->hdfile[ide->drive],addr,SEEK_SET);
                fwrite(ide->idebuffer,512,1,ide->hdfile[ide->drive]);
                ide_raise_irq(ide);
                ide->secount--;
                if (ide->secount)
                {
                        ide->atastat=0x08;
                        ide->pos=0;
                        ide->sector++;
                        if (ide->sector==(ide->spt[ide->drive]+1))
                        {
                                ide->sector=1;
                                ide->head++;
                                if (ide->head==(ide->hpc[ide->drive]))
                                {
                                        ide->head=0;
                                        ide->cylinder++;
                                }
                        }
                }
                else
                   ide->atastat=0x40;
                return;
                case 0x40: /*Read verify*/
                case 0x41:
                ide->pos=0;
                ide->atastat=0x40;
//                rpclog("Read verify callback %i %i %i offset %08X %i left\n",ide->sector,ide->cylinder,ide->head,addr,ide->secount);
                ide_raise_irq(ide);
                return;
                case 0x50: /*Format track*/
                addr=(((ide->cylinder*ide->hpc[ide->drive])+ide->head)*ide->spt[ide->drive])*512;
                if (!ide->skip512[ide->drive]) addr-=512;
//                rpclog("Format cyl %i head %i offset %08X secount %I\n",ide->cylinder,ide->head,addr,ide->secount);
                fseek(ide->hdfile[ide->drive],addr,SEEK_SET);
                memset(ide->idebufferb,0,512);
                for (c=0;c<ide->secount;c++)
                {
                        fwrite(ide->idebuffer,512,1,ide->hdfile[ide->drive]);
                }
                ide->atastat=0x40;
                ide_raise_irq(ide);
                return;
                case 0x91: /*Set parameters*/
                ide->spt[ide->drive]=ide->secount;
                ide->hpc[ide->drive]=ide->head+1;
                ide->cyl[ide->drive] = (ide->def_cyl[ide->drive] * ide->def_hpc[ide->drive] * ide->def_spt[ide->drive]) /
                                       (ide->hpc[ide->drive] * ide->spt[ide->drive]);
                ide->atastat=0x40;
                ide_raise_irq(ide);
                return;
                case 0xA1:
                case 0xE3:
                        case 0xE5:
                ide->atastat=0x41;
                ide->error=4;
                ide_raise_irq(ide);
                return;
                case 0xEC:
//                        rpclog("Callback EC\n");
                memset(ide->idebuffer,0,512);
                ide->idebuffer[1] = ide->def_cyl[ide->drive]; /*Cylinders*/
                ide->idebuffer[3] = ide->def_hpc[ide->drive];  /*Heads*/
                ide->idebuffer[6] = ide->def_spt[ide->drive];  /*Sectors*/
                for (addr=10;addr<20;addr++)
                    ide->idebuffer[addr]=0x2020;
                ide->idebuffer[10]=0x3030;
                for (addr=23;addr<47;addr++)
                    ide->idebuffer[addr]=0x2020;
                ide->idebufferb[46^1]='v'; /*Firmware version*/
                ide->idebufferb[47^1]='2';
                ide->idebufferb[48^1]='.';
                ide->idebufferb[49^1]='0';
                ide->idebufferb[54^1]='A'; /*Drive model*/
                ide->idebufferb[55^1]='r';
                ide->idebufferb[56^1]='c';
                ide->idebufferb[57^1]='u';
                ide->idebufferb[58^1]='l';
                ide->idebufferb[59^1]='a';
                ide->idebufferb[60^1]='t';
                ide->idebufferb[61^1]='o';
                ide->idebufferb[62^1]='r';
                ide->idebufferb[63^1]='H';
                ide->idebufferb[64^1]='D';
                ide->idebuffer[50]=0x4000; /*Capabilities*/
                ide->idebuffer[53] = 1;
                ide->idebuffer[54] = ide->cyl[ide->drive];
                ide->idebuffer[55] = ide->hpc[ide->drive];
                ide->idebuffer[56] = ide->spt[ide->drive];
                ide->idebuffer[57] = (ide->cyl[ide->drive] * ide->hpc[ide->drive] * ide->spt[ide->drive]) & 0xffff;
                ide->idebuffer[58] = (ide->cyl[ide->drive] * ide->hpc[ide->drive] * ide->spt[ide->drive]) >> 16;
                ide->pos=0;
                ide->atastat=0x08;
//                rpclog("ID callback\n");
                ide_raise_irq(ide);
                return;
        }
}
/*Read 1F1*/
/*Error &108A1 - parameters not recognised*/
