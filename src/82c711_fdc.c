/*Arculator 2.2 by Sarah Walker
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

static fdc_funcs_t c82c711_fdc_funcs;

static void c82c711_fdc_callback(void *p);
static int c82c711_fdc_getdata(int last, void *p);

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

	int discint;
	int paramstogo;
	int fdc_reset_stat;

	emu_timer_t timer;

	void *p;
} FDC;

static FDC _fdc;

static void arc_fdc_irq(int state, void *p)
{
	if (state)
		ioc_irqb(IOC_IRQB_DISC_IRQ);
	else
		ioc_irqbc(IOC_IRQB_DISC_IRQ);
}

static void arc_fdc_index_irq(void *p)
{
	ioc_irqa(IOC_IRQA_DISC_INDEX);
}

static void arc_fdc_fiq(int state, void *p)
{
	if (state)
		ioc_fiq(IOC_FIQ_DISC_DATA);
	else
		ioc_fiqc(IOC_FIQ_DISC_DATA);
}

static void (*c82711_fdc_irq)(int state, void *p);
static void (*c82711_fdc_index_irq)(void *p);
static void (*c82711_fdc_fiq)(int state, void *p);

static void c82c711_fdc_reset(void *p)
{
	FDC *fdc = (FDC *)p;

	disc_stop(0);
	disc_stop(1);
	disc_set_density(0);

	fdc->stat = 0x80;
	fdc->pnum = fdc->ptot = 0;
	fdc->st0 = 0xC0;
	fdc->lock = 0;
	fdc->head = 0;

	fdc->inread = 0;
	rpclog("Reset 82c711\n");
}

void c82c711_fdc_init(void)
{
	if (fdctype == FDC_82C711)
	{
		FDC *fdc = &_fdc;
		c82c711_fdc_reset(fdc);

		rpclog("82c711 present\n");
		timer_add(&fdc->timer, c82c711_fdc_callback, fdc, 0);
		fdc_funcs = &c82c711_fdc_funcs;
		fdc_timer = &fdc->timer;
		fdc_p = fdc;
		fdc_overridden = 0;

		c82711_fdc_irq = arc_fdc_irq;
		c82711_fdc_index_irq = arc_fdc_index_irq;
		c82711_fdc_fiq = arc_fdc_fiq;
		fdc->p = NULL;
	}
}

void *c82c711_fdc_init_override(void (*fdc_irq)(int state, void *p),
			       void (*fdc_index_irq)(void *p),
			       void (*fdc_fiq)(int state, void *p), void *p)
{
	FDC *fdc = malloc(sizeof(FDC));
	memset(fdc, 0, sizeof(FDC));

	c82c711_fdc_reset(fdc);

	timer_add(&fdc->timer, c82c711_fdc_callback, fdc, 0);
	fdc_funcs = &c82c711_fdc_funcs;
	fdc_timer = &fdc->timer;
	fdc_p = fdc;
	fdc_overridden = 1;

	c82711_fdc_irq = fdc_irq;
	c82711_fdc_index_irq = fdc_index_irq;
	c82711_fdc_fiq = fdc_fiq;
	fdc->p = p;

	return fdc;
}

static void c82c711_fdc_spindown(void *p)
{
//        rpclog("82c711 spindown\n");
	motoron = 0;
}

void c82c711_fdc_write(uint16_t addr, uint8_t val, void *p)
{
	FDC *fdc = p ? (FDC *)p : &_fdc;

//        rpclog("Write FDC %04X %02X %07X  %02X rate=%i\n", addr, val, PC, fdc.st0, fdc.rate);
	switch (addr&7)
	{
		case 1: return;
		case 2: /*DOR*/
//                printf("DOR was %02X\n",fdc.dor);
		if (val&4)
		{
			fdc->stat = 0x80;
			fdc->pnum = fdc->ptot = 0;
		}
		if ((val & 4) && !(fdc->dor & 4))
		{
//                        rpclog("Reset through 3f2\n");
			c82c711_fdc_reset(fdc);
			timer_set_delay_u64(&fdc->timer, 16 * TIMER_USEC);
			fdc->discint = -1;
		}
		if (fdc == fdc_p)
		{
			motoron = (val & 0xf0) ? 1 : 0;
			disc_set_motor(motoron);
			disc_drivesel = val & 3;
		}
		fdc->dor = val;
//                rpclog("DOR now %02X\n",val);
		return;
		case 4:
		if (val & 0x80)
		{
//                        rpclog("Reset through 3f4\n");
			c82c711_fdc_reset(fdc);
			timer_set_delay_u64(&fdc->timer, 16 * TIMER_USEC);
			fdc->discint = -1;
		}
		return;
		case 5: /*Command register*/
//                if (fdc.inread)
//                        rpclog("c82c711_fdc_write : writing while inread! %02X\n", val);
//                rpclog("Write command reg %i %i\n",fdc.pnum, fdc.ptot);
		if (fdc->pnum == fdc->ptot)
		{
			fdc->tc = 0;
			fdc->data_ready = 0;

			fdc->command = val;
//                        rpclog("Starting FDC command %02X\n",fdc.command);
			switch (fdc->command & 0x1F)
			{
				case 3: /*Specify*/
				fdc->pnum = 0;
				fdc->ptot = 2;
				fdc->stat = 0x90;
				break;
				case 4: /*Sense drive status*/
				fdc->pnum = 0;
				fdc->ptot = 1;
				fdc->stat = 0x90;
				break;
				case 5: /*Write data*/
//                                printf("Write data!\n");
				fdc->pnum = 0;
				fdc->ptot = 8;
				fdc->stat = 0x90;
				fdc->pos =  0;
//                                readflash=1;
				break;
				case 6: /*Read data*/
				fdc->pnum = 0;
				fdc->ptot = 8;
				fdc->stat = 0x90;
				fdc->pos = 0;
				break;
				case 7: /*Recalibrate*/
				fdc->pnum = 0;
				fdc->ptot = 1;
				fdc->stat = 0x90;
				break;
				case 8: /*Sense interrupt status*/
//                                printf("Sense interrupt status %i\n",curdrive);
				fdc->lastdrive = curdrive;
				fdc->discint = 8;
				fdc->pos = 0;
				c82c711_fdc_callback(fdc);
				break;
				case 10: /*Read sector ID*/
				fdc->pnum = 0;
				fdc->ptot = 1;
				fdc->stat = 0x90;
				fdc->pos = 0;
				break;
				case 0x0d: /*Format track*/
				fdc->pnum = 0;
				fdc->ptot = 5;
				fdc->stat = 0x90;
				fdc->pos = 0;
				fdc->format_state = 0;
				break;
				case 15: /*Seek*/
				fdc->pnum = 0;
				fdc->ptot = 2;
				fdc->stat = 0x90;
				break;
				case 0x0e: /*Dump registers*/
				fdc->lastdrive = curdrive;
				fdc->discint = 0x0e;
				fdc->pos = 0;
				c82c711_fdc_callback(fdc);
				break;
				case 0x10: /*Get version*/
				fdc->lastdrive = curdrive;
				fdc->discint = 0x10;
				fdc->pos = 0;
				c82c711_fdc_callback(fdc);
				break;
				case 0x12: /*Set perpendicular mode*/
				fdc->pnum = 0;
				fdc->ptot = 1;
				fdc->stat = 0x90;
				fdc->pos = 0;
				break;
				case 0x13: /*Configure*/
				fdc->pnum = 0;
				fdc->ptot = 3;
				fdc->stat = 0x90;
				fdc->pos = 0;
				break;
				case 0x14: /*Unlock*/
				case 0x94: /*Lock*/
				fdc->lastdrive = curdrive;
				fdc->discint = fdc->command;
				fdc->pos = 0;
				c82c711_fdc_callback(fdc);
				break;

				case 0x18:
				fdc->stat = 0x10;
				fdc->discint  = 0xfc;
				c82c711_fdc_callback(fdc);
				break;

				default:
#ifndef RELEASE_BUILD
				fatal("Bad FDC command %02X\n",val);
#endif
				fdc->stat = 0x10;
				fdc->discint = 0xfc;
				timer_set_delay_u64(&fdc->timer, 25 * TIMER_USEC);
				break;
			}
		}
		else
		{
			fdc->params[fdc->pnum++] = val;
			if (fdc->pnum == fdc->ptot)
			{
//                                rpclog("Got all params %02X\n", fdc.command);
				fdc->stat = 0x30;
				fdc->discint = fdc->command&0x1F;
				timer_set_delay_u64(&fdc->timer, 128 * TIMER_USEC);
				curdrive = fdc->params[0] & 3;
				switch (fdc->discint)
				{
					case 5: /*Write data*/
					fdc->track[curdrive] = fdc->params[1];
					fdc->head = fdc->params[2];
					fdc->sector = fdc->params[3];
					fdc->eot[curdrive] = fdc->params[4];
					if (fdc == fdc_p)
						disc_writesector(curdrive, fdc->sector, fdc->track[curdrive], fdc->head, fdc->density);
					timer_disable(&fdc->timer);
					fdc->written = 0;
					readflash[curdrive] = 1;
					fdc->pos = 0;
					c82711_fdc_fiq(1, fdc->p);
					break;

					case 6: /*Read data*/
					fdc->track[curdrive] = fdc->params[1];
					fdc->head = fdc->params[2];
					fdc->sector = fdc->params[3];
					fdc->eot[curdrive] = fdc->params[4];
					if (fdc == fdc_p)
						disc_readsector(curdrive, fdc->sector, fdc->track[curdrive], fdc->head, fdc->density);
					timer_disable(&fdc->timer);
					readflash[curdrive] = 1;
					fdc->inread = 1;
					break;

					case 7: /*Recalibrate*/
					fdc->stat =  1 << curdrive;
					timer_disable(&fdc->timer);
					if (fdc == fdc_p)
						disc_seek(curdrive, 0);
					break;

					case 0x0d: /*Format*/
					fdc->format_state = 1;
					fdc->pos = 0;
					fdc->stat = 0x30;
					break;

					case 0xf: /*Seek*/
					fdc->stat =  1 << curdrive;
					fdc->head = (fdc->params[0] & 4) ? 1 : 0;
					timer_disable(&fdc->timer);
					if (fdc == fdc_p)
						disc_seek(curdrive, fdc->params[1]);
					break;

					case 10: /*Read sector ID*/
					timer_disable(&fdc->timer);
					fdc->head = (fdc->params[0] & 4) ? 1 : 0;
//                                        rpclog("Read sector ID %i %i\n", fdc->rate, curdrive);
					if (fdc == fdc_p)
						disc_readaddress(curdrive, fdc->track[curdrive], fdc->head, fdc->density);
					break;
				}
			}
		}
		return;
		case 7:
		fdc->rate = val & 3;
		switch (val & 3)
		{
			case 0: fdc->density = 2; break;
			case 1: case 2: fdc->density = 1; break;
			case 3: fdc->density = 3; break;
		}
		if (fdc == fdc_p)
			disc_set_density(fdc->density);
//                rpclog("FDC rate = %i\n", val & 3);
		return;
	}
//        printf("Write FDC %04X %02X\n",addr,val);
//        dumpregs();
//        exit(-1);
}

uint8_t c82c711_fdc_read(uint16_t addr, void *p)
{
	FDC *fdc = p ? (FDC *)p : &_fdc;
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
		c82711_fdc_irq(0, fdc->p);
		temp = fdc->stat;
		break;
		case 5: /*Data*/
		fdc->stat &= ~0x80;
		if (fdc->paramstogo)
		{
			fdc->paramstogo--;
			temp = fdc->res[10 - fdc->paramstogo];
//                        rpclog("Read param %i %02X\n",10-fdc->paramstogo,temp);
			if (!fdc->paramstogo)
			{
				fdc->stat = 0x80;
//                                fdc->st0=0;
			}
			else
			{
				fdc->stat |= 0xC0;
//                                c82c711_fdc_callback(NULL);
			}
		}
		else
		{
			temp = fdc->dat;
			fdc->data_ready = 0;
		}
		if (fdc->discint == 0xA)
		{
			timer_set_delay_u64(&fdc->timer, 128 * TIMER_USEC);
		}
		fdc->stat &= 0xf0;
		break;
		case 7: /*Disk change*/
//                rpclog("FDC read 3f7\n");
		if (fdc == fdc_p)
		{
			if (fdc->dor & (0x10 << (fdc->dor & 3)))
				temp = (discchange[fdc->dor & 3] || disc_empty(fdc->dor & 3)) ? 0x80 : 0;
			else
				temp = 0;
		}
		else
			temp = 0x80;
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

static void c82c711_fdc_callback(void *p)
{
	FDC *fdc = (FDC *)p;
	int temp;
//        if (fdc.inread)
//                rpclog("c82c711_fdc_callback : while inread! %08X %i %02X  %i\n", fdc->discint, curdrive, fdc.st0, ins);
	switch (fdc->discint)
	{
		case -3: /*End of command with interrupt*/
//                if (output) printf("EOC - interrupt!\n");
//rpclog("EOC\n");
		c82711_fdc_irq(1, fdc->p);
		case -2: /*End of command*/
		fdc->stat = (fdc->stat & 0xf) | 0x80;
		return;
		case -1: /*Reset*/
//rpclog("Reset\n");
		c82711_fdc_irq(1, fdc->p);
		fdc->fdc_reset_stat = 4;
		return;
		case 3: /*Specify*/
		fdc->stat=0x80;
		fdc->specify[0] = fdc->params[0];
		fdc->specify[1] = fdc->params[1];
		return;
		case 4: /*Sense drive status*/
		fdc->res[10] = (fdc->params[0] & 7) | 0x28;
		if (!fdc->track[curdrive])
			fdc->res[10] |= 0x10;
		fdc->stat = (fdc->stat & 0xf) | 0xd0;
		fdc->paramstogo = 1;
		fdc->discint = 0;
		return;
		case 5: /*Write data*/
		readflash[curdrive] = 1;
		fdc->sector++;
		if (fdc->sector > fdc->params[5])
		{
			fdc->sector = 1;
			if (fdc->command & 0x80)
			{
				if (fdc->head)
				{
					fdc->tc = 1;
					fdc->track[curdrive]++;
				}
				else
					fdc->head ^= 1;
			}
			else
			{
				fdc->tc = 1;
				fdc->track[curdrive]++;
			}
		}
		if (fdc->tc)
		{
			fdc->discint = -2;
			c82711_fdc_irq(1, fdc->p);
			fdc->stat = 0xD0;
			fdc->res[4] = (fdc->head ? 4 : 0) | curdrive;
			fdc->res[5] = fdc->res[6] = 0;
			fdc->res[7] = fdc->track[curdrive];
			fdc->res[8] = fdc->head;
			fdc->res[9] = fdc->sector;
			fdc->res[10] = fdc->params[4];
			fdc->paramstogo=7;
			return;
		}
		disc_writesector(curdrive, fdc->sector, fdc->track[curdrive], fdc->head, fdc->density);
		c82711_fdc_fiq(1, fdc->p);
		return;
		case 6: /*Read data*/
//                rpclog("Read data %i\n", fdc->tc);
		readflash[curdrive] = 1;
		fdc->sector++;
		if (fdc->sector > fdc->params[5])
		{
			fdc->sector = 1;
			if (fdc->command & 0x80)
			{
				if (fdc->head)
				{
					fdc->tc = 1;
					fdc->track[curdrive]++;
				}
				else
					fdc->head ^= 1;
			}
			else
			{
				fdc->tc = 1;
				fdc->track[curdrive]++;
			}
		}
		if (fdc->tc)
		{
			fdc->inread = 0;
			fdc->discint = -2;
			c82711_fdc_irq(1, fdc->p);
			fdc->stat = 0xD0;
			fdc->res[4] = (fdc->head ? 4 : 0) | curdrive;
			fdc->res[5] = fdc->res[6] = 0;
			fdc->res[7] = fdc->track[curdrive];
			fdc->res[8] = fdc->head;
			fdc->res[9] = fdc->sector;
			fdc->res[10] = fdc->params[4];
			fdc->paramstogo = 7;
			return;
		}
		disc_readsector(curdrive, fdc->sector, fdc->track[curdrive], fdc->head, fdc->density);
		fdc->inread = 1;
		return;

		case 7: /*Recalibrate*/
		fdc->track[curdrive] = 0;
//                if (!driveempty[fdc.dor & 1]) discchanged[fdc.dor & 1] = 0;
		fdc->st0 = 0x20 | curdrive | (fdc->head ? 4 : 0);
		fdc->discint = -3;
		timer_advance_u64(&fdc->timer, 256 * TIMER_USEC);
//                printf("Recalibrate complete!\n");
		fdc->stat = 0x80 | (1 << curdrive);
		return;

		case 8: /*Sense interrupt status*/
//                pclog("Sense interrupt status %i\n", fdc->fdc_reset_stat);

		fdc->dat = fdc->st0;

		if (fdc->fdc_reset_stat)
		{
			fdc->st0 = (fdc->st0 & 0xf8) | (4 - fdc->fdc_reset_stat) | (fdc->head ? 4 : 0);
			fdc->fdc_reset_stat--;
		}
		fdc->stat    = (fdc->stat & 0xf) | 0xd0;
		fdc->res[9]  = fdc->st0;
		fdc->res[10] = fdc->track[curdrive];
		if (!fdc->fdc_reset_stat)
			fdc->st0 = 0x80;

		fdc->paramstogo = 2;
		fdc->discint = 0;
		return;

		case 0x0d: /*Format track*/
//                rpclog("Format\n");
		if (fdc->format_state == 1)
		{
			c82711_fdc_fiq(1, fdc->p);
			fdc->format_state = 2;
			timer_advance_u64(&fdc->timer, 16 * TIMER_USEC);
		}
		else if (fdc->format_state == 2)
		{
			temp = c82c711_fdc_getdata(fdc->pos == ((fdc->params[2] * 4) - 1), fdc);
			if (temp == -1)
			{
				timer_advance_u64(&fdc->timer, 16 * TIMER_USEC);
				return;
			}
			fdc->format_dat[fdc->pos++] = temp;
			if (fdc->pos == (fdc->params[2] * 4))
				fdc->format_state = 3;
			timer_advance_u64(&fdc->timer, 16 * TIMER_USEC);
		}
		else if (fdc->format_state == 4)
		{
//                        rpclog("Format next stage\n");
			disc_format(curdrive, fdc->track[curdrive], fdc->head, fdc->density);
			fdc->format_state = 4;
		}
		else
		{
			fdc->discint = -2;
			c82711_fdc_irq(1, fdc->p);
			fdc->stat = 0xD0;
			fdc->res[4] = (fdc->head ? 4 : 0) | curdrive;
			fdc->res[5] = fdc->res[6] = 0;
			fdc->res[7] = fdc->track[curdrive];
			fdc->res[8] = fdc->head;
			fdc->res[9] = fdc->format_dat[fdc->pos - 2] + 1;
			fdc->res[10] = fdc->params[4];
			fdc->paramstogo=7;
			fdc->format_state = 0;
			return;
		}
		return;

		case 15: /*Seek*/
		fdc->track[curdrive] = fdc->params[1];
//                if (!driveempty[fdc.dor & 1]) discchanged[fdc.dor & 1] = 0;
//                printf("Seeked to track %i %i\n",fdc.track[curdrive], curdrive);
		fdc->st0 = 0x20 | curdrive | (fdc->head ? 4 : 0);
		fdc->discint = -3;
		timer_advance_u64(&fdc->timer, 256 * TIMER_USEC);
		fdc->stat = 0x80 | (1 << curdrive);
//                pclog("Stat %02X ST0 %02X\n", fdc.stat, fdc.st0);
		return;
		case 0x0e: /*Dump registers*/
		fdc->stat = (fdc->stat & 0xf) | 0xd0;
		fdc->res[3] = fdc->track[0];
		fdc->res[4] = fdc->track[1];
		fdc->res[5] = 0;
		fdc->res[6] = 0;
		fdc->res[7] = fdc->specify[0];
		fdc->res[8] = fdc->specify[1];
		fdc->res[9] = fdc->eot[curdrive];
		fdc->res[10] = (fdc->perp & 0x7f) | ((fdc->lock) ? 0x80 : 0);
		fdc->paramstogo = 10;
		fdc->discint = 0;
		return;

		case 0x10: /*Version*/
		fdc->stat = (fdc->stat & 0xf) | 0xd0;
		fdc->res[10] = 0x90;
		fdc->paramstogo = 1;
		fdc->discint = 0;
		return;

		case 0x12:
		fdc->perp = fdc->params[0];
		fdc->stat = 0x80;
//                picint(0x40);
		return;
		case 0x13: /*Configure*/
		fdc->config = fdc->params[1];
		fdc->pretrk = fdc->params[2];
		fdc->stat = 0x80;
//                picint(0x40);
		return;
		case 0x14: /*Unlock*/
		fdc->lock = 0;
		fdc->stat = (fdc->stat & 0xf) | 0xd0;
		fdc->res[10] = 0;
		fdc->paramstogo = 1;
		fdc->discint = 0;
		return;
		case 0x94: /*Lock*/
		fdc->lock = 1;
		fdc->stat = (fdc->stat & 0xf) | 0xd0;
		fdc->res[10] = 0x10;
		fdc->paramstogo = 1;
		fdc->discint = 0;
		return;


		case 0xfc: /*Invalid*/
		fdc->dat = fdc->st0 = 0x80;
//                pclog("Inv!\n");
		//picint(0x40);
		fdc->stat = (fdc->stat & 0xf) | 0xd0;
//                fdc->stat|=0xC0;
		fdc->res[10] = fdc->st0;
		fdc->paramstogo = 1;
		fdc->discint = 0;
		return;
	}
//        printf("Bad FDC disc int %i\n",fdc->discint);
//        dumpregs();
//        exit(-1);
}

static void fdc_overrun(void *p)
{
	FDC *fdc = (FDC *)p;

	disc_stop(curdrive);
	timer_disable(&fdc->timer);

	c82711_fdc_irq(1, fdc->p);
	fdc->stat = 0xD0;
	fdc->res[4] = 0x40 | (fdc->head ? 4 : 0) | curdrive;
	fdc->res[5] = 0x10; /*Overrun*/
	fdc->res[6] = 0;
	fdc->res[7] = fdc->track[curdrive];
	fdc->res[8] = fdc->head;
	fdc->res[9] = fdc->sector;
	fdc->res[10] = fdc->params[4];
	fdc->paramstogo = 7;
}

static void c82c711_fdc_data(uint8_t dat, void *p)
{
	FDC *fdc = (FDC *)p;

	if (fdc->tc)
		return;
//        rpclog("Data time : %i cycles  %i\n", lastcycles - cycles, cycles);
//        lastcycles = cycles;
	if (fdc->data_ready)
	{
//                rpclog("Overrun\n");
		fdc_overrun(fdc);
	}
	else
	{
//        rpclog("fdc_data %02X %i\n", dat, fdc_time);
		fdc->dma_dat = dat;
		fdc->data_ready = 1;
		c82711_fdc_fiq(1, fdc->p);
	}
}

static void c82c711_fdc_finishread(void *p)
{
	FDC *fdc = (FDC *)p;

	fdc->inread = 0;
	timer_set_delay_u64(&fdc->timer, 25 * TIMER_USEC);
//        rpclog("fdc_finishread\n");
}

static void c82c711_fdc_notfound(void *p)
{
	FDC *fdc = (FDC *)p;

	timer_disable(&fdc->timer);

	c82711_fdc_irq(1, fdc->p);
	fdc->stat = 0xD0;
	fdc->res[4] = 0x40 | (fdc->head ? 4 : 0) | curdrive;
	fdc->res[5] = 5;
	fdc->res[6] = 0;
	fdc->res[7] = 0;
	fdc->res[8] = 0;
	fdc->res[9] = 0;
	fdc->res[10] = 0;
	fdc->paramstogo = 7;
//        rpclog("c82c711_fdc_notfound\n");
}

static void c82c711_fdc_datacrcerror(void *p)
{
	FDC *fdc = (FDC *)p;

	timer_disable(&fdc->timer);

	c82711_fdc_irq(1, fdc->p);
	fdc->stat = 0xD0;
	fdc->res[4] = 0x40 | (fdc->head ? 4 : 0) | curdrive;
	fdc->res[5] = 0x20; /*Data error*/
	fdc->res[6] = 0x20; /*Data error in data field*/
	fdc->res[7] = fdc->track[curdrive];
	fdc->res[8] = fdc->head;
	fdc->res[9] = fdc->sector;
	fdc->res[10] = fdc->params[4];
	fdc->paramstogo = 7;
//        rpclog("c82c711_fdc_datacrcerror\n");
}

static void c82c711_fdc_headercrcerror(void *p)
{
	FDC *fdc = (FDC *)p;

	timer_disable(&fdc->timer);

	c82711_fdc_irq(1, fdc->p);
	fdc->stat = 0xD0;
	fdc->res[4] = 0x40 | (fdc->head ? 4 : 0) | curdrive;
	fdc->res[5] = 0x20; /*Data error*/
	fdc->res[6] = 0;
	fdc->res[7] = fdc->track[curdrive];
	fdc->res[8] = fdc->head;
	fdc->res[9] = fdc->sector;
	fdc->res[10] = fdc->params[4];
	fdc->paramstogo = 7;
//        rpclog("c82c711_fdc_headercrcerror\n");
}

static void c82c711_fdc_writeprotect(void *p)
{
	FDC *fdc = (FDC *)p;

	timer_disable(&fdc->timer);

	c82711_fdc_irq(1, fdc->p);
	fdc->stat = 0xD0;
	fdc->res[4] = 0x40 | (fdc->head ? 4 : 0) | curdrive;
	fdc->res[5] = 0x02; /*Not writeable*/
	fdc->res[6] = 0;
	fdc->res[7] = 0;
	fdc->res[8] = 0;
	fdc->res[9] = 0;
	fdc->res[10] = 0;
	fdc->paramstogo = 7;
}

static int c82c711_fdc_getdata(int last, void *p)
{
	FDC *fdc = (FDC *)p;
	uint8_t temp;
//        rpclog("Disc get data\n");
	if (!fdc->written && !fdc->tc)
		return -1;
	if (!last && !fdc->tc)
		c82711_fdc_fiq(1, fdc->p);
	fdc->written = 0;
	temp = fdc->dma_dat;
	fdc->dma_dat = 0;
	return temp;
}

static void c82c711_fdc_sectorid(uint8_t track, uint8_t side, uint8_t sector, uint8_t size, uint8_t crc1, uint8_t crc2, void *p)
{
	FDC *fdc = (FDC *)p;
//        rpclog("SectorID %i %i %i %i\n", track, side, sector, size);
	c82711_fdc_irq(1, fdc->p);
	fdc->stat = 0xD0;
	fdc->res[4] = (fdc->head ? 4 : 0) | curdrive;
	fdc->res[5] = 0;
	fdc->res[6] = 0;
	fdc->res[7] = track;
	fdc->res[8] = side;
	fdc->res[9] = sector;
	fdc->res[10] = size;
	fdc->paramstogo = 7;
}

static void c82c711_fdc_indexpulse(void *p)
{
	FDC *fdc = (FDC *)p;

	c82711_fdc_index_irq(fdc->p);
//        rpclog("c82c711_fdc_indexpulse\n");
}

uint8_t c82c711_fdc_dmaread(int tc, void *p)
{
	FDC *fdc = p ? (FDC *)p : &_fdc;
//        rpclog("DMA read %08X %02X\n", addr, fdc->dma_dat);
	fdc->data_ready = 0;
	if (tc) /*Terminal count*/
		fdc->tc = 1;
	c82711_fdc_fiq(0, fdc->p);
	return fdc->dma_dat;
}

void c82c711_fdc_dmawrite(uint8_t val, int tc, void *p)
{
	FDC *fdc = p ? (FDC *)p : &_fdc;

	if (tc) /*Terminal count*/
		fdc->tc = 1;
	fdc->dma_dat = val;
	c82711_fdc_fiq(0, fdc->p);
	fdc->written = 1;
}

static fdc_funcs_t c82c711_fdc_funcs =
{
	.data           = c82c711_fdc_data,
	.spindown       = c82c711_fdc_spindown,
	.finishread     = c82c711_fdc_finishread,
	.notfound       = c82c711_fdc_notfound,
	.datacrcerror   = c82c711_fdc_datacrcerror,
	.headercrcerror = c82c711_fdc_headercrcerror,
	.writeprotect   = c82c711_fdc_writeprotect,
	.getdata        = c82c711_fdc_getdata,
	.sectorid       = c82c711_fdc_sectorid,
	.indexpulse     = c82c711_fdc_indexpulse
};
