/*Arculator 2.0 by Sarah Walker
  ST-506 hard disc controller emulation
  This is used by both the built-in 4x0 interface, and the AKD52 podule*/
#include <stdio.h>
#include <stdlib.h>
#include "arc.h"
#include "config.h"
#include "ioc.h"
#include "st506.h"
#include "timer.h"

static void st506_callback(void *p);

#define BUSY            0x80
#define PARAMREJECT     0x40
#define COMEND          0x20
#define SEEKEND         0x10
#define DRIVEERROR      0x08
#define ABNEND          0x04

int st506_present;
static st506_t internal_st506;

void st506_init(st506_t *st506, char *fn_pri, int pri_spt, int pri_hpc, char *fn_sec, int sec_spt, int sec_hpc, void (*irq_raise)(st506_t *st506), void (*irq_clear)(st506_t *st506), void *p)
{
        st506->status = 0;
        st506->rp = st506->wp = 0;
        st506->drq = 0;
        st506->first = 0;
        st506->hdfile[0] = fopen(fn_pri, "rb+");
        st506->spt[0] = pri_spt;
        st506->hpc[0] = pri_hpc;
        st506->hdfile[1] = fopen(fn_sec, "rb+");
        st506->spt[1] = sec_spt;
        st506->hpc[1] = sec_hpc;
        timer_add(&st506->timer, st506_callback, st506, 0);
        st506->irq_raise = irq_raise;
        st506->irq_clear = irq_clear;
        st506->p = p;
}
void st506_close(st506_t *st506)
{
        if (st506->hdfile[0])
                fclose(st506->hdfile[0]);
        if (st506->hdfile[1])
                fclose(st506->hdfile[1]);
}

#define TOV 0x58
#define IPH 0x3C
#define NSC 0x24
#define NRY 0x20

static void st506_updateinterrupts(st506_t *st506)
{
        if ((st506->status & ~st506->OM1 & 0x38) || st506->drq)
                st506->irq_raise(st506);
        else
                st506->irq_clear(st506);
//        rpclog("ST506 status %i  %02X %02X %i\n", (st506->status & ~st506->OM1 & 0x38) || st506->drq, st506->status, st506->OM1, st506->drq);
//        if (ioc.irqb&8 && !oldirq) rpclog("HDC IRQ\n");
}

static void st506_error(st506_t *st506, uint8_t err)
{
//        rpclog("ST506 error - %02X\n",err);
        st506->status = ABNEND | COMEND;
        st506->drq = 0;
        st506_updateinterrupts(st506);
        st506->ssb = err;
}

static void readdataerror(st506_t *st506)
{
        int c;
        
        for (c = 9; c >= 0; c--)
                st506->param[c + 2] = st506->param[c];
        st506->param[0] = st506->param[1] = 0;
        st506->rp = st506->wp = 0;
        st506->param[1] = st506->ssb;
}

static int check_chs_params(st506_t *st506, int drive)
{
        if (st506->lcyl > 1023)
        {
                st506_error(st506, NSC);
                readdataerror(st506);
                return 1;
        }
        if (st506->lhead >= st506->hpc[drive])
        {
                st506_error(st506, IPH);
                readdataerror(st506);
                return 1;
        }
        if (st506->lsect >= st506->spt[drive])
        {
                st506_error(st506, TOV);
                readdataerror(st506);
                return 1;
        }
        return 0;
}


int st506writes=0;
void st506_writeb(st506_t *st506, uint32_t a, uint8_t v)
{
//        return;
        rpclog("Write HDC %08X %08X\n",a,v);
#ifndef RELEASE_BUILD
        fatal("ST506 write %08X %02X %02X at %07X\n",a,a&0x3C,v,PC);
#endif
}

uint8_t st506_readb(st506_t *st506, uint32_t a)
{
        uint16_t temp;
//        return 0xFF;
//        st506writes++;
//        return 0xFF;
        switch (a & 0x3C)
        {
                case 0x24: /*Data read*/
                if (st506->rp < 16)
                {
                        temp = st506->param[st506->rp++] << 8;
                        temp |= st506->param[st506->rp++];
//                        st506->p+=2;
//                        rpclog("Reading params - returning %04X\n",temp);
//                        rpclog("Read HDC %08X %08X %02X\n",a,temp,ioc.irqb);
                        return temp;//st506->param[st506->p-1];
                }
                else if (st506->rp < 272)
                {
                        temp = st506->buffer[st506->rp++] << 8;
                        temp |= st506->buffer[st506->rp++];
                        if (st506->rp == 272)
                        {
                                st506->drq = 0;
                                st506_updateinterrupts(st506);
                                timer_set_delay_u64(&st506->timer, 5000 * TIMER_USEC);
                        }
//                        rpclog("Read HDC %08X %08X %07X %02X\n",a,temp,PC,ioc.irqb);
                        return temp;
                }
                else
                {
                        st506->drq = 0;
                        st506_updateinterrupts(st506);
//                        rpclog("Read HDC %08X %08X %02X\n",a,0x23,ioc.irqb);
                        return 0x23;
                }
                break;
        }
#ifndef RELEASE_BUILD
        fatal("ST506 read %08X %02X at %07X\n",a,a&0x3C,PC);
#endif
        return 0xff;
}

void st506_writel(st506_t *st506, uint32_t a, uint32_t v)
{
        uint8_t temp;
//        output=0;
//        return;
//        v>>=16;
//        rpclog("Write HDC %08X %08X %07X %02X\n",a&0x3C,v,PC,ioc.irqb);
        st506writes++;
        switch (a & 0x3C)
        {
                case 0: /*New command*/
//                output=0;
//                rpclog("New ST506 command %04X\n",v);
                if ((st506->status) & BUSY && (v != 0xF0))
                {
                        rpclog("Command rejected\n");
                        return;
                }
                st506->drq = 0;
                if (v != 0xF0)
                        st506->status = PARAMREJECT;
                st506->wp = st506->rp = 0;
                st506->command = v;
                if (v != 8 && v != 0xF0)
                        st506->ssb = 0;
                switch (v)
                {
                        case 0:
                        return;
                        
                        case 0x08: /*Recall*/
                        st506->status = 0;
                        st506_updateinterrupts(st506);
                        return;
                        
                        case 0x10: /*Enable polling*/
                        st506->status &= ~PARAMREJECT;
                        return;
                        
                        case 0x18: /*Disable polling*/
                        st506->status &= ~PARAMREJECT;
                        return;
                        
                        case 0x28: /*Check drive*/
                        if (st506->param[0] != 1 && st506->param[0] != 2)
                        {
                                st506_error(st506, NRY);
                                readdataerror(st506);
                                return;
                        }
                        st506->drive = st506->param[0] - 1;
                        temp = st506->param[0] & 3;
                        st506->status = 0;
                        st506->param[0] = 0;
                        st506->param[1] = 0;
                        st506->param[2] = temp;
                        st506->param[3] = 0;
                        st506->param[4] = st506->track[st506->drive] ? 0xC0 : 0xE0;
                        st506->param[5] = 0;
                        return;
                        
                        case 0x40: /*Read data*/
                        rpclog("Read data %02X %02X\n",st506->param[0],st506->param[1]);
                        if (st506->param[0] != 1 && st506->param[0] != 2)
                        {
                                st506_error(st506, NRY);
                                readdataerror(st506);
                                return;
                        }
                        st506->drive = st506->param[0] - 1;
                        st506->lcyl = (st506->param[2] << 8) | st506->param[3];
                        st506->lhead = st506->param[4];
                        st506->lsect = st506->param[5];
                        st506->oplen = (st506->param[6] << 8) | st506->param[7];
                        rpclog("Read data : cylinder %i head %i sector %i   length %i sectors\n",st506->lcyl,st506->lhead,st506->lsect,st506->oplen);
                        if (check_chs_params(st506, st506->drive))
                                return;
                        fseek(st506->hdfile[st506->drive], (((((st506->lcyl*st506->hpc[st506->drive])+st506->lhead)*st506->spt[st506->drive])+st506->lsect)*256), SEEK_SET);
//                        rpclog("Seeked to %08X\n",(((((st506->lcyl*8)+st506->lhead)*32)+st506->lsect)*256));
                        timer_set_delay_u64(&st506->timer, 5000 * TIMER_USEC);
                        st506->status |= 0x80;
                        return;
                        
                        case 0x48: /*Check data*/
                        if (st506->param[0] != 1 && st506->param[0] != 2)
                        {
                                st506_error(st506, NRY);
                                readdataerror(st506);
                                return;
                        }
                        st506->drive = st506->param[0] - 1;
                        st506->lcyl = (st506->param[2] << 8) | st506->param[3];
                        st506->lhead = st506->param[4];
                        st506->lsect = st506->param[5];
                        st506->oplen = (st506->param[6] << 8) | st506->param[7];
                        rpclog("Check data : cylinder %i head %i sector %i   length %i sectors\n",st506->lcyl,st506->lhead,st506->lsect,st506->oplen);
                        if (check_chs_params(st506, st506->drive))
                                return;
                        fseek(st506->hdfile[st506->drive], (((((st506->lcyl*st506->hpc[st506->drive])+st506->lhead)*st506->spt[st506->drive])+st506->lsect)*256), SEEK_SET);
                        timer_set_delay_u64(&st506->timer, 5000 * TIMER_USEC);
                        st506->status |= 0x80;
                        return;
                        
                        case 0x87: /*Write data*/
                        if (st506->param[0] != 1 && st506->param[0] != 2)
                        {
                                st506_error(st506, NRY);
                                readdataerror(st506);
                                return;
                        }
                        st506->drive = st506->param[0] - 1;
                        st506->lcyl = (st506->param[2] << 8) | st506->param[3];
                        st506->lhead = st506->param[4];
                        st506->lsect = st506->param[5];
                        st506->oplen = (st506->param[6] << 8) | st506->param[7];
                        rpclog("Write data : cylinder %i head %i sector %i   length %i sectors\n",st506->lcyl,st506->lhead,st506->lsect,st506->oplen);
                        if (check_chs_params(st506, st506->drive))
                                return;
                        fseek(st506->hdfile[st506->drive], (((((st506->lcyl*st506->hpc[st506->drive])+st506->lhead)*st506->spt[st506->drive])+st506->lsect)*256), SEEK_SET);
//                        rpclog("Seeked to %08X\n",(((((st506->lcyl*8)+st506->lhead)*32)+st506->lsect)*256));
                        timer_set_delay_u64(&st506->timer, 5000 * TIMER_USEC);
                        st506->status |= 0x80;
                        st506->first = 1;
                        return;
                        
                        case 0xA3: /*Write Format*/
                        if (st506->param[0] != 1 && st506->param[0] != 2)
                        {
                                st506_error(st506, NRY);
                                readdataerror(st506);
                                return;
                        }
                        st506->drive = st506->param[0] - 1;
                        st506->lcyl = st506->track[st506->drive];
                        st506->lhead = st506->param[1];
                        st506->lsect = 0;
                        st506->oplen = (st506->param[2] << 8) | st506->param[3];
                        rpclog("Write format : drive %i cylinder %i head %i sector %i   length %i sectors\n",st506->drive,st506->lcyl,st506->lhead,st506->lsect,st506->oplen);
                        if (check_chs_params(st506, st506->drive))
                                return;
                        fseek(st506->hdfile[st506->drive], (((((st506->lcyl*st506->hpc[st506->drive])+st506->lhead)*st506->spt[st506->drive])+st506->lsect)*256), SEEK_SET);
                        timer_set_delay_u64(&st506->timer, 5000 * TIMER_USEC);
                        st506->status |= 0x80;
                        st506->first = 1;
                        return;

                        case 0xC0: /*Seek*/
                        if (st506->param[0] != 1 && st506->param[0] != 2)
                        {
                                st506_error(st506, NRY);
                                readdataerror(st506);
                                return;
                        }
                        st506->drive = st506->param[0] - 1;
                        st506->track[st506->drive] = st506->param[3] | (st506->param[2] << 8);
                        rpclog("Seek drive %i to track %i\n",st506->drive, st506->track);
                        st506->param[0] = 0;
                        st506->param[1] = 0;
                        st506->param[2] = 0;
                        st506->param[3] = st506->cul;
                        st506->status |= COMEND;
                        st506->status |= SEEKEND;
                        st506_updateinterrupts(st506);
                        return;

                        case 0xC8: /*Recalibrate*/
                        if (st506->param[0] != 1 && st506->param[0] != 2)
                        {
                                st506_error(st506, NRY);
                                readdataerror(st506);
                                return;
                        }
                        st506->drive = st506->param[0] - 1;
                        st506->track[st506->drive] = 0;
//                        rpclog("Recalibrate : seek to track %i\n",st506->track);
                        st506->status |= SEEKEND;
                        st506->param[0] = 0;
                        st506->param[1] = 0;
                        st506->param[2] = 0;
                        st506->param[3] = st506->cul;
                        return;

                        case 0xE8: /*Specify*/
//                        rpclog("Specify\nOM1 = %02X\nSHRL = %02X\nSectors = %i\nHeads = %i\nCylinders = %i\n",st506->param[1],st506->param[8],st506->param[7]+1,st506->param[6]+1,(st506->param[5]|((st506->param[4]&3)<<8))+1);
                        st506->status = PARAMREJECT;
                        st506->OM1 = st506->param[1];
                        st506->cul = st506->param[3];
                        st506->param[0] = 0;
                        st506->param[1] = 0;
//                        rpclog("OM1=%02X\n",st506->OM1);
                        return;

                        case 0xF0: /*Abort*/
                        st506->status = (st506->status & PARAMREJECT) | COMEND;
                        st506->param[0] = 0;
                        st506->param[1] = 4;
                        st506_updateinterrupts(st506);
                        break;
                        
                        case 0xFF:
                        break;
                        
#ifndef RELEASE_BUILD
                        default:
                        fatal("Bad ST506 command %02X\n",v);
#endif
                }
                return;
                
                case 0x04: /*Params*/
                if (st506->wp < 16)
                {
                        st506->param[st506->wp] = v >> 8;
                        st506->param[st506->wp+1] = v;
//                        rpclog("Writing params - pointer %02X param %02X%02X\n",st506->wp,v>>8,v&0xFF);
                        st506->wp += 2;
                        if (st506->wp >= 16)
                                st506->status |= PARAMREJECT;
                }
                return;
                
                case 0x28: case 0x2C: /*DMA write*/
//                rpclog("Write DMA %i\n",st506->rp);
                if (st506->rp >= 16 && st506->rp < 272)
                {
                        st506->buffer[st506->rp++] = v >> 8;
                        st506->buffer[st506->rp++] = v;
                        if (st506->rp == 272)
                        {
                                st506->drq = 0;
                                st506_updateinterrupts(st506);
                                timer_set_delay_u64(&st506->timer, 5000 * TIMER_USEC);
                        }
//                        rpclog("Write HDC %08X %08X %i %07X %i %02X  %02X %02X\n",a,temp,st506->p,PC,st506->drq,ioc.irqb,st506->status,st506->OM1);
                        return;
                }
                else if (st506->rp > 16)
                {
                        st506->drq = 0;
                        st506_updateinterrupts(st506);
//                        rpclog("Write HDC %08X %08X   %07X\n",a,1223,PC);
                }
                return;
        }
#ifndef RELEASE_BUILD
        fatal("ST506 writel %08X %02X %08X at %07X\n",a,a&0x3C,v,PC);
#endif
}

uint32_t st506_readl(st506_t *st506, uint32_t a)
{
        uint16_t temp;

        switch (a & 0x3C)
        {
                case 0x08: case 0x0C: /*DMA read*/
//                rpclog("Read DMA %i\n",st506->p);
                if (st506->rp >= 16 && st506->rp < 272)
                {
                        temp = st506->buffer[st506->rp++] << 8;
                        temp |= st506->buffer[st506->rp++];
                        if (st506->rp == 272)
                        {
                                st506->drq = 0;
                                st506_updateinterrupts(st506);
                                timer_set_delay_u64(&st506->timer, 5000 * TIMER_USEC);
                        }
//                        rpclog("Read HDC %08X %08X %i %07X\n",a,temp,st506->rp,PC);
                        return temp;
                }
                else if (st506->rp > 16)
                {
                        st506->drq = 0;
                        st506_updateinterrupts(st506);
//                        rpclog("Read HDC %08X %08X   %07X\n",a,1223,PC);
                        return 0x1223;
                }
                return 0;
                
                case 0x20:
//                rpclog("Return ST506 status %04X %07X %02X %02X\n",st506->status<<8,PC,ioc.irqb,ioc.mskb);
//                output=1;
//                timetolive=6;
//                rpclog("Read HDC %08X %08X %07X\n",a,st506->status<<8,PC);
                return st506->status << 8;
                
                case 0x24: /*Params*/
                if (st506->rp < 16)
                {
                        temp = st506->param[st506->rp++] << 8;
                        temp |= st506->param[st506->rp++];
//                        st506->p+=2;
//                        rpclog("Reading params - returning %04X\n",temp);
//                        rpclog("Read HDC %08X %08X %02X\n",a,temp,ioc.irqb);
                        return temp;//st506->param[st506->p-1];
                }
                return 0xFF;
        }
#ifndef RELEASE_BUILD
        fatal("ST506 readl %08X %02X at %07X\n",a,a&0x3C,PC);
#endif
        return 0xffff;
}

static void st506_callback(void *p)
{
        st506_t *st506 = p;
        int c;
        uint8_t temp;
//        return;
//        rpclog("Callback!\n");
        switch (st506->command)
        {
                case 0x40: /*Read sector*/
                if (st506->oplen)
                {
//                        if (st506->lsect>31)  { st506error(TOV); readdataerror(); return; }
                        st506->lsect++;
                        if (st506->lsect == st506->spt[st506->drive])
                        {
                                st506->lsect = 0;
                                st506->lhead++;
                                if (st506->lhead == st506->hpc[st506->drive])
                                {
                                        st506->lhead = 0;
                                        st506->lcyl++;
#ifndef RELEASE_BUILD
                                        if (st506->lcyl > 1023)
                                                fatal("Hit limit\n");
#endif
                                }
                        }
//                        rpclog("Reading from pos %08X - %i sectors left\n",ftell(st506->hdfile[st506->drive]),st506->oplen);
                        st506->oplen--;
//                        rpclog("Read ST506buffer from %08X\n",ftell(hdfile));
                        fread(st506->buffer+16, 256, 1, st506->hdfile[st506->drive]);
//                        if ((ftell(hdfile)-256)==0x2048C00) dumpst506buffer();
                        for (c = 16; c < 272; c += 2)
                        {
                                temp = st506->buffer[c];
                                st506->buffer[c] = st506->buffer[c+1];
                                st506->buffer[c+1] = temp;
                        }
                        st506->rp = 16;
                        st506->drq = 1;
                        st506_updateinterrupts(st506);
//                        rpclog("HDC interrupt part\n");
                }
                else
                {
                        for (c = 9; c >= 0; c--)
                                st506->param[c+2] = st506->param[c];
                                
                        st506->param[0] = st506->param[1] = 0;
                        st506->rp = st506->wp = 0;
//                        rpclog("Finished read sector! %02X\n",st506->OM1);
                        st506->status |= COMEND | PARAMREJECT;
                        st506->status &= ~0x80;
                        st506->drq = 0;
                        st506_updateinterrupts(st506);
//                        rpclog("HDC interrupt full\n");
                }
                break;
                
                case 0x48: /*Check data*/
//                rpclog("Check data %i\n",st506->oplen);
                if (st506->oplen)
                {
                        st506->lsect++;
                        if (st506->lsect == st506->spt[st506->drive])
                        {
                                st506->lsect = 0;
                                st506->lhead++;
                                if (st506->lhead == st506->hpc[st506->drive])
                                {
                                        st506->lhead = 0;
                                        st506->lcyl++;
#ifndef RELEASE_BUILD
                                        if (st506->lcyl > 1023)
                                                fatal("Hit limit\n");
#endif
                                }
                        }
                        st506->oplen--;
                        timer_set_delay_u64(&st506->timer, 5000 * TIMER_USEC);
//                        rpclog("Check data next callback\n");
                }
                else
                {
                        for (c = 9; c >= 0; c--)
                                st506->param[c+2] = st506->param[c];
                                
                        st506->param[0] = st506->param[1] = 0;
                        st506->rp = st506->wp = 0;
                        st506->status |= COMEND | PARAMREJECT;
                        st506->status &= ~0x80;
                        st506->drq = 0;
                        st506_updateinterrupts(st506);
//                        rpclog("Check data over\n");
                }
                break;
                
                case 0x87: /*Write sector*/
                if (st506->first)
                {
                        st506->rp = 16;
                        st506->drq = 1;
                        st506_updateinterrupts(st506);
//                        rpclog("Write HDC interrupt first\n");
                        st506->first = 0;
                }

                else
                {
                        st506->lsect++;
                        if (st506->lsect == st506->spt[st506->drive])
                        {
                                st506->lsect = 0;
                                st506->lhead++;
                                if (st506->lhead == st506->hpc[st506->drive])
                                {
                                        st506->lhead = 0;
                                        st506->lcyl++;
#ifndef RELEASE_BUILD
                                        if (st506->lcyl > 1023)
                                                fatal("Hit limit\n");
#endif
                                }
                        }
                        st506->oplen--;
                        for (c = 16; c < 272; c += 2)
                        {
                                temp = st506->buffer[c];
                                st506->buffer[c] = st506->buffer[c+1];
                                st506->buffer[c+1] = temp;
                        }
//                        rpclog("Write ST506buffer to %08X\n",ftell(hdfile));
//                        if (ftell(hdfile)==0x2048C00) dumpst506buffer();
                        fwrite(st506->buffer+16, 256, 1, st506->hdfile[st506->drive]);
//                        rpclog("ST506 OPLEN %i\n",st506->oplen);
                        if (st506->oplen)
                        {
                                st506->rp = 16;
                                st506->drq = 1;
                                st506_updateinterrupts(st506);
//                                rpclog("Write HDC interrupt part\n");
                        }
                        else
                        {
                                for (c = 9; c >= 0; c--)
                                    st506->param[c+2] = st506->param[c];
                                    
                                st506->param[0] = st506->param[1] = 0;
                                st506->rp = st506->wp = 0;
//                                rpclog("Finished write sector! %02X\n",st506->OM1);
                                st506->status |= COMEND | PARAMREJECT;
                                st506->status &= ~0x80;
                                st506->drq = 0;
                                st506_updateinterrupts(st506);
//                                rpclog("Write HDC interrupt full\n");
//                                output=1;
                        }
                }
                break;
                case 0xA3: /*Write format*/
                if (st506->first)
                {
                        st506->rp = 16;
                        st506->drq = 1;
                        st506_updateinterrupts(st506);
//                        rpclog("Write HDC interrupt first\n");
                        st506->first = 0;
                }
                else
                {
                        c = 0;
                        while (c < 256 && st506->oplen)
                        {
                                st506->lsect++;
                                if (st506->lsect == st506->spt[st506->drive])
                                {
                                        st506->lsect = 0;
                                        st506->lhead++;
                                        if (st506->lhead == st506->hpc[st506->drive])
                                        {
                                                st506->lhead = 0;
                                                st506->lcyl++;
                                                if (st506->lcyl > 1023)
                                                {
                                                        error("Hit limit\n");
                                                        exit(-1);
                                                }
                                        }
                                }
                                st506->oplen--;
                                fwrite(st506->buffer+16, 256, 1, st506->hdfile[st506->drive]);
                                c+=4;
                        }
                        if (st506->oplen)
                        {
                                st506->rp = 16;
                                st506->drq = 1;
                                st506_updateinterrupts(st506);
                        }
                        else
                        {
                                for (c = 9; c >= 0; c--)
                                    st506->param[c+2] = st506->param[c];
                                    
                                st506->param[0] = st506->param[1] = 0;
                                st506->rp = st506->wp = 0;
                                st506->status |= COMEND | PARAMREJECT;
                                st506->status &= ~0x80;
                                st506->drq = 0;
                                st506_updateinterrupts(st506);
                        }
                }
                break;
        }
}

static void st506_internal_irq_raise(st506_t *st506)
{
        ioc_irqb(IOC_IRQB_ST506);
}
static void st506_internal_irq_clear(st506_t *st506)
{
        ioc_irqbc(IOC_IRQB_ST506);
}

void st506_internal_init(void)
{
        st506_init(&internal_st506, hd_fn[0], hd_spt[0], hd_hpc[0], hd_fn[1], hd_spt[1], hd_hpc[1], st506_internal_irq_raise, st506_internal_irq_clear, NULL);
}
void st506_internal_close(void)
{
        st506_close(&internal_st506);
}
uint8_t st506_internal_readb(uint32_t addr)
{
        return st506_readb(&internal_st506, addr);
}
uint32_t st506_internal_readl(uint32_t addr)
{
        return st506_readl(&internal_st506, addr);
}
void st506_internal_writeb(uint32_t addr, uint8_t val)
{
        return st506_writeb(&internal_st506, addr, val);
}
void st506_internal_writel(uint32_t addr, uint32_t val)
{
        return st506_writel(&internal_st506, addr, val);
}
