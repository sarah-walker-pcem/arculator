/*Arculator 2.0 by Sarah Walker
  MEMC1/MEMC1a emulation*/

int flybacklines;
#include <stdio.h>
#include <string.h>
#include "arc.h"
#include "ioc.h"
#include "mem.h"
#include "memc.h"
#include "soundopenal.h"
#include "timer.h"
#include "vidc.h"

int memc_videodma_enable;
int memc_refreshon;
int memc_refresh_always;
int memc_is_memc1 = 1;
int memc_type;

int memc_dma_sound_req;
uint64_t memc_dma_sound_req_ts;
int memc_dma_video_req;
uint64_t memc_dma_video_req_ts;
uint64_t memc_dma_video_req_start_ts;
uint64_t memc_dma_video_req_period;
int memc_dma_cursor_req;
uint64_t memc_dma_cursor_req_ts;

uint32_t memctrl;

int sdmaena=0;
int bigcyc=0;
int pagesize;
int memcpages[0x2000];
int spdcount;

uint32_t sstart,ssend,sptr;
uint32_t vinit,vstart,vend;
uint32_t cinit;

uint32_t spos,sendN,sstart2;
int nextvalid;
#define getdmaaddr(addr) (((addr>>2)&0x7FFF)<<2)
void writememc(uint32_t a)
{
//        rpclog("Write MEMC %08X\n",a);
        switch ((a>>17)&7)
        {
                case 0:
                LOG_MEMC_VIDEO("MEMC write %08X - VINIT  = %05X\n",a,getdmaaddr(a)*4);
                vinit=getdmaaddr(a);
                LOG_MEMC_VIDEO("Vinit write %08X %07X\n",vinit,PC);
                return;
                case 1:
                /*Set start of video RAM*/
                LOG_MEMC_VIDEO("MEMC write %08X - VSTART = %05X\n",a,getdmaaddr(a)*4);
                vstart=getdmaaddr(a);
                LOG_MEMC_VIDEO("Vstart write %08X %07X\n",vstart,PC);
                return;
                case 2:
                /*Set end of video RAM*/
                LOG_MEMC_VIDEO("MEMC write %08X - VEND   = %05X\n",a,getdmaaddr(a)*4);
                vend=getdmaaddr(a);
                LOG_MEMC_VIDEO("Vend write %08X %07X\n",vend,PC);
                return;
                case 3:
                LOG_MEMC_VIDEO("MEMC write %08X - CINIT  = %05X\n",a,getdmaaddr(a));
                cinit=getdmaaddr(a);
                LOG_MEMC_VIDEO("CINIT=%05X\n",cinit<<2);
                return;
                case 4:
//                rpclog("MEMC write %08X - SSTART = %05X %05X\n",a,getdmaaddr(a),spos);
                sstart=getdmaaddr(a); /*printf("SSTART=%05X\n",sstart<<2);*/

                if (!nextvalid) nextvalid=1;
                if (nextvalid==2) nextvalid=0;

                ioc_irqbc(IOC_IRQB_SOUND_BUFFER);
                nextvalid=2;
                return;
                case 5:
//                rpclog("MEMC write %08X - SEND   = %05X %05X\n",a,getdmaaddr(a),spos);
                sendN=getdmaaddr(a);

                if (nextvalid==1) nextvalid=2;
                if (nextvalid!=2) nextvalid=1;
                return;
                case 6:
//                rpclog("MEMC write %08X - SPTR   = %05X %05X\n",a,getdmaaddr(a),spos);
                sptr=getdmaaddr(a); /*printf("SPTR=%05X\n",sptr); */
                spos=sstart2=sstart<<2;
                ssend=sendN<<2;
                ioc_irqb(IOC_IRQB_SOUND_BUFFER);
                nextvalid=0;
                return;
                case 7: osmode=(a&0x1000)?1:0; /*MEMC ctrl*/
                sdmaena=(a&0x800)?1:0;
                pagesize=(a&0xC)>>2;
                resetpagesize(pagesize);
                memc_videodma_enable = a & 0x400;
                LOG_MEMC_VIDEO("MEMC set memc_videodma_enable = %d\n", memc_videodma_enable);
                switch ((a >> 6) & 3) /*High ROM speed*/
                {
                        case 0: /*450ns*/
                        mem_setromspeed(4, 4);
                        break;
                        case 1: /*325ns*/
                        mem_setromspeed(3, 3);
                        break;
                        case 2: /*200ns*/
                        mem_setromspeed(2, 2);
                        break;
                        case 3: /*200ns with 60ns nibble mode*/
                        mem_setromspeed(2, 1);
                        break;
                }
                memc_refreshon = (((a >> 8) & 3) == 1);
                memc_refresh_always = (((a >> 8) & 3) == 3);
                mem_dorefresh = (memc_refreshon && !vidc_displayon) || memc_refresh_always;
//                rpclog("MEMC ctrl write %08X %i  %i %i %i\n",a,sdmaena, memc_refreshon, memc_refresh_always, mem_dorefresh);
                return;
        }
}

void writecam(uint32_t a)
{
        int page,access,logical,c;
//        rpclog("Write CAM %08X pagesize %i %i\n",a,pagesize,ins);
        switch (pagesize)
        {
//                #if 0
                case 1: /*8k*/
                page=((a>>1)&0x3f) | ((a&1)<<6);
                access=(a>>8)&3;
                logical=(a>>13)&0x3FF;
                logical|=(a&0xC00);
//                rpclog("Map page %02X to %03X\n",page,logical);
                for (c=0;c<0x2000;c++)
                {
                        if ((memcpages[c]&~0x1FFF)==(page<<13))
                        {
                                memcpages[c]=~0;
                                memstat[c]=0;
                        }
                }
                logical<<=1;
                for (c=0;c<2;c++)
                {
                        memcpages[logical+c]=page<<13;
                        memstat[logical+c]=access+1;
                        mempoint[logical+c]=&ram[(page<<11)+(c<<10)];
                        mempointb[logical+c]=(uint8_t *)&ram[(page<<11)+(c<<10)];
                }
                break;
//                #endif
                case 2: /*16k*/
                page=((a>>2)&0x1f) | ((a&3)<<5);
                access=(a>>8)&3;
                logical=(a>>14)&0x1FF;
                logical|=(a>>1)&0x600;
                for (c=0;c<0x2000;c++)
                {
                        if ((memcpages[c]&~0x3FFF)==(page<<14))
                        {
                                memcpages[c]=~0;
                                memstat[c]=0;
                        }
                }
                logical<<=2;
                for (c=0;c<4;c++)
                {
                        memcpages[logical+c]=page<<14;
                        memstat[logical+c]=access+1;
                        mempoint[logical+c]=&ram[(page<<12)+(c<<10)];
                        mempointb[logical+c]=(uint8_t *)&ram[(page<<12)+(c<<10)];
                }
                break;
                case 3: /*32k*/
                page=((a>>3)&0xf) | ((a&1)<<4) | ((a&2)<<5) | ((a&4)<<3);
                if (a&0x80) page|=0x80;
                if (a&0x1000) page|=0x100;
                access=(a>>8)&3;
                logical=(a>>15)&0xFF;
                logical|=(a>>2)&0x300;
//                printf("Mapping %08X to %08X\n",0x2000000+(page*32768),logical<<15);
                for (c=0;c<0x2000;c++)
                {
                        if ((memcpages[c]&~0x7FFF)==(page<<15))
                        {
                                memcpages[c]=~0;
                                memstat[c]=0;
                        }
                }
                logical<<=3;
                for (c=0;c<8;c++)
                {
                        memcpages[logical+c]=page<<15;
                        memstat[logical+c]=access+1;
                        mempoint[logical+c]=&ram[(page<<13)+(c<<10)];
                        mempointb[logical+c]=(uint8_t *)&ram[(page<<13)+(c<<10)];
                }
                break;
        }
//        memcpermissions[logical]=access;
}

void initmemc()
{
        int c;
        for (c=0;c<0x2000;c++) memstat[c]=0;
        for (c=0x3400;c<0x3800;c++) memstat[c]=0;
}
