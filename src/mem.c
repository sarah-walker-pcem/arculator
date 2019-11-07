/*Arculator 2.0 by Sarah Walker
  Memory read/write functions*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arc.h"
#include "arm.h"
#include "82c711.h"
#include "82c711_fdc.h"
#include "config.h"
#include "cp15.h"
#include "eterna.h"
#include "ide.h"
#include "ioc.h"
#include "ioeb.h"
#include "joystick.h"
#include "memc.h"
#include "podules.h"
#include "printer.h"
#include "st506.h"
#include "vidc.h"
#include "wd1770.h"

uint64_t mem_speed[16384][2];

int mem_dorefresh;

uint64_t mem_spd_multi;

static int mem_romspeed_n, mem_romspeed_s;


uint8_t backplane_mask;
int bank;
int ddensity;
// static char err2[256];
FILE *olog;
int fdcside;
int realmemsize;

uint32_t *ram,*rom;
uint8_t *romb;
uint32_t *mempoint[0x4000];
uint8_t *mempointb[0x4000];
int memstat[0x4000];
int memmode;

void initmem(int memsize)
{
        int c,d=(memsize>>2)-1;

        rpclog("initmem %i\n", memsize);
        realmemsize=memsize;
        ram=(uint32_t *)malloc(memsize*1024);
        rom=(uint32_t *)malloc(0x200000);
        romb = (uint8_t *)rom;
        for (c=0;c<0x4000;c++) memstat[c]=0;
        for (c=0x2000;c<0x3000;c++) memstat[c]=3;
        for (c=0x2000;c<0x3000;c++) mempoint[c]=&ram[(c&d)<<10];
        for (c=0x3800;c<0x4000;c++) memstat[c]=5;
        for (c=0x3800;c<0x4000;c++) mempoint[c]=&rom[(c&0x1FF)<<10];
        memset(ram,0,memsize*1024);
        memstat[0]=1;
        mempoint[0]=rom;
        for (c=0;c<0x4000;c++) mempointb[c]=(uint8_t *)mempoint[c];
        realmemsize=memsize;

        mem_spd_multi = arm_has_cp15 ? (((uint64_t)speed_mhz << 32) / arm_mem_speed) : (1ull << 32);
        for (c = 0; c < 0x3000; c++)
        {
                mem_speed[c][0] = 1 * mem_spd_multi;
                mem_speed[c][1] = 2 * mem_spd_multi;
        }
        for (c = 0x3000; c < 0x3800; c++)
                mem_speed[c][0] = mem_speed[c][1] = 2 * mem_spd_multi;
        for (c = 0x3800; c < 0x4000; c++)
                mem_speed[c][0] = mem_speed[c][1] = 4 * mem_spd_multi;
        mem_romspeed_n = mem_romspeed_s = 4;
        rpclog("Update2: mem=%i,%i\n", mem_speed[0x1800][0], mem_speed[0x1800][1]);
}

void mem_setromspeed(int n, int s)
{
        int c;

        mem_romspeed_n = n;
        mem_romspeed_s = s;
        
/*        if (cp15_cacheon)
        {
                n -= (n >> 1);
                if (!n) n = 1;
                s -= (s >> 1);
                if (!s) s = 1;
        }*/
        mem_spd_multi = arm_has_cp15 ? (((uint64_t)speed_mhz << 32) / arm_mem_speed) : (1ull << 32);
        for (c = 0x3800; c < 0x4000; c++)
        {
                mem_speed[c][0] = s * mem_spd_multi;
                mem_speed[c][1] = n * mem_spd_multi;
        }
        
        rpclog("mem_setromspeed %i %i\n", n, s);
}

void mem_updatetimings()
{
        int c;

        mem_spd_multi = arm_has_cp15 ? (((uint64_t)speed_mhz << 32) / arm_mem_speed) : (1ull << 32);

        for (c = 0; c < 0x3000; c++)
        {
        	mem_speed[c][0] = 1 * mem_spd_multi;
                mem_speed[c][1] = 2 * mem_spd_multi;
        }
        rpclog("Update: mem=%i,%i\n", mem_speed[0x1800][0], mem_speed[0x1800][1]);

        mem_setromspeed(mem_romspeed_n, mem_romspeed_s);
}

void resizemem(int memsize) /*memsize is 4096,8192,16384*/
{
        int c,d=(memsize>>2)-1;
        rpclog("resizemem %i\n", memsize);
        free(ram);
        ram=(uint32_t *)malloc(memsize*1024);
        for (c=0x2000;c<0x3000;c++) mempoint[c]=&ram[(c&d)<<10];
        for (c=0x2000;c<0x3000;c++) mempointb[c]=(uint8_t *)mempoint[c];
        memset(ram,0,memsize*1024);
        realmemsize=memsize;
        for (c=0;c<0x4000;c++) mempointb[c]=(uint8_t *)mempoint[c];
}

void resetpagesize(int pagesize)
{
        int c,d=(realmemsize>>2)-1,e;
//        rpclog("Resetpagesize %i %i %i %04X\n",pagesize,realmemsize,ins,d);
        if (pagesize==3 && realmemsize==2048)
        {
                for (c=0x2000;c<0x3000;c++)
                {
                        e=c&1023;
                        e=(e&1)|((e&~3)<<1);
                        mempoint[c]=&ram[((e&0x3FF)<<10)];
                }
                for (c=0x2000;c<0x3000;c++) mempointb[c]=(uint8_t *)mempoint[c];
        }
        else if (pagesize==3 && realmemsize==1024)
        {
                for (c=0x2000;c<0x3000;c++)
                {
                        e=c&511;
                        e=(e&1)|((e&~3)<<1);
                        mempoint[c]=&ram[((e&0x1FF)<<10)];
                }
                for (c=0x2000;c<0x3000;c++) mempointb[c]=(uint8_t *)mempoint[c];
        }
        else if (pagesize==3 && realmemsize==512)
        {
                for (c=0x2000;c<0x3000;c++)
                {
                        e=c&255;
                        e=(e&1)|((e&~3)<<1);
                        mempoint[c]=&ram[((e&0xFF)<<10)];
                }
                for (c=0x2000;c<0x3000;c++) mempointb[c]=(uint8_t *)mempoint[c];
        }
        else
        {
                for (c=0x2000;c<0x3000;c++) mempoint[c]=&ram[(c&d)<<10];
                for (c=0x2000;c<0x3000;c++) mempointb[c]=(uint8_t *)mempoint[c];
        }
}

uint32_t readmemf(uint32_t a)
{
        a&=0x3FFFFFC;
//        if (a&0xFC000000) { rpclog("Databort readmemf %08X\n",a); databort=2; return 0xdeadbeef; }
        switch (a>>20)
        {
//#if 0
                case 0x00: case 0x01: case 0x02: case 0x03: /*RAM*/
                case 0x04: case 0x05: case 0x06: case 0x07:
                case 0x08: case 0x09: case 0x0A: case 0x0B:
                case 0x0C: case 0x0D: case 0x0E: case 0x0F:
                case 0x10: case 0x11: case 0x12: case 0x13:
                case 0x14: case 0x15: case 0x16: case 0x17:
                case 0x18: case 0x19: case 0x1A: case 0x1B:
                case 0x1C: case 0x1D: case 0x1E: case 0x1F:
                prefabort_next=1;
                return 0;
                case 0x3F:
                        return 0xFFFFFFFF;
//#endif
        }
        prefabort_next=1;
        rpclog("Prefabort %08X\n",a);
        return 0xdeadbeef;
/*        sprintf(err2,"Bad fetch %06X %03X %04X\n",a,a>>15,a&0x7FFF);
        MessageBox(NULL,err2,"Arc",MB_OK);
        dumpregs();
        exit(-1);*/
}

uint8_t readmemfb(uint32_t a)
{
//        if (a==0x1800F42)
/*        if (a==(0x1801010))
        {
                rpclog("Read byte %08X %07X\n",a,PC);
                return mempointb[((a)>>15)&0x7FF][((a)&0x7FFF)];
        }*/
//        if (((a)&~3)==0x2D8D0)
/*        if (a==0x7DC)
        {
                if (!olog) olog=fopen("olog.txt","wt");
                sprintf(err2,"Read byte %08X %07X\n",a,PC);
                fputs(err2,olog);
                return mempointb[((a)>>15)&0x7FF][((a)&0x7FFF)];
        }*/
/*        if (((a)&~0x7FFF)==0x8000)
        {
                rpclog("Read byte %08X %07X %02X  R2=%08X R4=%08X R5=%08X\n",a,PC,mempointb[((a)>>15)&0x7FF][((a)&0x7FFF)],armregs[2],armregs[4],armregs[5]);
                return mempointb[((a)>>15)&0x7FF][((a)&0x7FFF)];
        }*/
        a &= 0x3ffffff;
/*        if ((a&~0xFFF)==0x8000)
        {
                printf("Read %04X %08X %02X\n",a,PC,mempointb[((a)>>15)&0x7FF][(a)&0x7FFF]);
                return mempointb[((a)>>15)&0x7FF][(a)&0x7FFF];
        }*/
        switch (a>>20)
        {
                case 0x30:
                case 0x31:
                case 0x32: /*IOC*/
                case 0x33:
                if (!(a & ((1 << 16) | (1 << 17) | (1 << 21)))) /*MEMC podule space*/
                        return podule_memc_read_b((a & 0xc000) >> 14, a & 0x3fff);

                if (fdctype == FDC_82C711 && a >= 0x3012000 && a <= 0x302A000)
                        return c82c711_fdc_dmaread(a);

                bank=(a>>16)&7;

                switch (bank)
                {
                        case 0: /*IOC*/
                        if (a & (1 << 21))
                                return ioc_read(a);
                        return 0xff;
                        case 1: /*1772 FDC*/
                        if (fdctype == FDC_WD1770)
                                return wd1770_read(a);
                        return c82c711_read(a);
                        case 2: /*Econet*/
                        if (joystick_rtfm_present)
                                return joystick_rtfm_read(a);
                        return 0xFF;
                        case 3: /*Serial*/
                        return 0xFF;
                        case 4: /*Internal podules*/
/*                        if (romset>3)
                        {
                                if ((a&~0x1F)==0x33c0004) return readeterna(a);
                                if ((a&~0x1F)==0x3340000) return readeterna(a);
                                if ((a&~0x1F)==0x33C0000) return readeterna(a);
                        }*/
                        return podule_read_b((a & 0xC000) >> 14, a & 0x3FFF);
                        case 5: 
                        if (fdctype == FDC_82C711) /*IOEB*/
                        {
                                return ioeb_read(a);
                        }
                        else /*Internal latches*/
                        {
                                switch (a&0xFFFC)
                                {
                                        case 0x0010: return 0xFF; /*Printer*/
                                        case 0x0018:
                                        return 0xFF; /*FDC Latch B*/
                                        case 0x0008: case 0x000C:
                                        case 0x0020: case 0x0024: case 0x0028: case 0x002C:
                                        if (st506_present)
                                                return st506_internal_readb(a);
                                        return 0xFF;
                                        case 0x0040: /*FDC Latch A*/
                                        return 0xFF;
                                        case 0x0048: return 0xFF; /*????*/
                                        case 0x0050: return 0;
                                        case 0x0074: return 0xFF; /*????*/
                                }
                        }
                        case 6: /*Backplane*/
                        switch (a&0xFFFC)
                        {
                                case 0x0000:
                                return podule_irq_state();

                                case 0x0004:
                                return podule_read_backplane_mask();
                        }
                        break;
                }
                return 0xFF;
        }
//        rpclog("Data abort b %07X\n",a);
        databort=1;
        LOG_DATABORT("Dat abort readb %07X %07X\n",a,PC);
        return 0xef;
/*        sprintf(err2,"Bad read byte %06X %03X %04X\n",a,a>>15,a&0x7FFF);
        MessageBox(NULL,err2,"Arc",MB_OK);
        dumpregs();
        exit(-1);*/
}

uint32_t readmemfl(uint32_t a)
{
//        uint32_t temp;
/*        if (a==0x7DC)
        {
                if (!olog) olog=fopen("olog.txt","wt");
                sprintf(err2,"Read word %08X %07X\n",a,PC);
                fputs(err2,olog);
                return mempointb[((a)>>15)&0x7FF][((a)&0x7FFF)];
        }*/
/*        if (((a)&~0x7FFF)==0x8000)
        {
                rpclog("Read long %08X %07X %08X R2=%08X R4=%08X R5=%08X\n",a,PC,mempoint[((a)>>15)&0x7FF][((a)&0x7FFF)>>2],armregs[2],armregs[4],armregs[5]);
                return mempoint[((a)>>15)&0x7FF][((a)&0x7FFF)>>2];
        }*/
        a &= 0x3ffffff;
        switch (a>>20)
        {
#if 0
                case 0x00: case 0x01: case 0x02: case 0x03: /*RAM*/
                case 0x04: case 0x05: case 0x06: case 0x07:
                case 0x08: case 0x09: case 0x0A: case 0x0B:
                case 0x0C: case 0x0D: case 0x0E: case 0x0F:
                case 0x10: case 0x11: case 0x12: case 0x13:
                case 0x14: case 0x15: case 0x16: case 0x17:
                case 0x18: case 0x19: case 0x1A: case 0x1B:
                case 0x1C: case 0x1D: case 0x1E: case 0x1F:
                databort=1;
                return 0xdeadbeef;
#endif

                case 0x30:
                case 0x31:
                case 0x32: /*IOC*/
                case 0x33:
                if (!(a & ((1 << 16) | (1 << 17) | (1 << 21)))) /*MEMC podule space*/
                        return podule_memc_read_w((a & 0xc000) >> 14, a & 0x3fff);

                if (fdctype == FDC_82C711 && a >= 0x3012000 && a <= 0x302A000)
                        return c82c711_fdc_dmaread(a);

//                        if (output) rpclog("IOC space readl %08X\n",a);
                bank=(a>>16)&7;
                switch (bank)
                {
                        case 0: /*IOC*/
                        if (a & (1 << 21))
                                return ioc_read(a);
                        return 0xff;
                        case 1: /*1772 FDC*/
                        if (fdctype == FDC_WD1770)
                                return wd1770_read(a);
                        if ((a&0xFFF)==0x7C0)
                                return readidew(&ide_internal);
                        return c82c711_read(a);
                        case 2: /*Econet*/
                        if (joystick_rtfm_present)
                                return joystick_rtfm_read(a);
                        return 0xFFFF;
                        case 3: /*Serial*/
                        return 0xFFFF;
                        case 4: /*Internal podules*/
/*                        if (romset>3)
                        {
                                if ((a&~0x1F)==0x33c0004) return readeterna(a);
                                if ((a&~0x1F)==0x3340000) return readeterna(a);
                                if ((a&~0x1F)==0x33C0000) return readeterna(a);
                        }*/
                        return podule_read_w((a & 0xC000) >> 14, a & 0x3FFF);
                        case 5: /*IOEB*/
                        if (fdctype == FDC_82C711)
                        {
                                return ioeb_read(a);
                        }
                        else /*Internal latches*/
                        {
                                switch (a&0xFFFC)
                                {
                                        case 0x0010: return 0xFFFF; /*Printer*/
                                        case 0x0018:
                                        return 0xFFFF; /*FDC Latch B*/
                                        case 0x0008: case 0x000C:
                                        case 0x0020: case 0x0024: case 0x0028: case 0x002C:
                                        if (st506_present)
                                                return st506_internal_readl(a);
                                        return 0xFFFFFFFF;
                                        case 0x0040: /*FDC Latch A*/
                                        return 0xFFFF;
                                        case 0x0048: return 0xFFFF; /*????*/
                                        case 0x0050: return 0;
                                        case 0x0074: return 0xFFFF; /*????*/
                                        case 0x0078: return 0xFFFF; /*????*/
                                }
                        }
                        break;
                        case 6: /*Backplane*/
                        switch (a&0xFFFC)
                        {
                                case 0x0000:
                                return podule_irq_state();

                                case 0x0004:
                                return podule_read_backplane_mask();
                        }
                        break;
                }
                return 0xFFFF;
                case 0x34: case 0x35: case 0x36: case 0x37: /*Expansion ROMs*/
                return 0xFFFFFFFF;
        }
//        rpclog("Data abort l %07X\n",a);
        databort=1;
        LOG_DATABORT("Dat abort readl %07X %07X\n",a,PC);
        return 0xdeadbeef;
/*        sprintf(err2,"Bad read long %06X %03X %04X\n",a,a>>15,a&0x7FFF);
        MessageBox(NULL,err2,"Arc",MB_OK);
        dumpregs();
        exit(-1);*/
}

int f42count=0;

void writememfb(uint32_t a,uint8_t v)
{
        int bank;

        a &= 0x3ffffff;
        switch (a>>20)
        {
#if 0
                case 0x00: case 0x01: case 0x02: case 0x03: /*RAM*/
                case 0x04: case 0x05: case 0x06: case 0x07:
                case 0x08: case 0x09: case 0x0A: case 0x0B:
                case 0x0C: case 0x0D: case 0x0E: case 0x0F:
                case 0x10: case 0x11: case 0x12: case 0x13:
                case 0x14: case 0x15: case 0x16: case 0x17:
                case 0x18: case 0x19: case 0x1A: case 0x1B:
                case 0x1C: case 0x1D: case 0x1E: case 0x1F:
                databort=1;
                return;
#endif
//                case 0x1F: return; if (a>=0x1f08000 && a<=0x1f0ffff) return; break;

                case 0x30:
                case 0x31:
                case 0x32: /*IOC*/
                case 0x33:
                if (!(a & ((1 << 16) | (1 << 17) | (1 << 21)))) /*MEMC podule space*/
                {
                        podule_memc_write_b((a & 0xc000) >> 14, a & 0x3fff, v);
                        return;
                }

                if (fdctype == FDC_82C711 && a >= 0x3012000 && a <= 0x302A000)
                {
                        c82c711_fdc_dmawrite(a, v);
                        return;
		}

//                rpclog("IOC space writeb %08X %02X\n",a,v);
                bank=(a>>16)&7;
                switch (bank)
                {
                        case 0: /*IOC*/
                        if (a & (1 << 21))
                                ioc_write(a, v);
                        return;
                        case 1: /*1772 FDC*/
                        if (fdctype == FDC_WD1770)
                                wd1770_write(a,v);
                        else
                                c82c711_write(a,v);
                        return;
                        case 2: /*Econet*/
                        return;
                        case 3: /*Serial*/
                        return;
                        case 4: /*Internal podules*/
/*                        if (romset>3)
                        {
                                if ((a&~0x1F)==0x33c0004) { writeeterna(a,v); return; }
                                if ((a&~0x1F)==0x3340000) { writeeterna(a,v); return; }
                                if ((a&~0x1F)==0x33C0000) { writeeterna(a,v); return; }
                        }*/
                        podule_write_b((a & 0xC000) >> 14, a & 0x3FFF, v);
                        return;
                        case 5: /*IOEB*/
                        if (fdctype == FDC_82C711)
                        {
                                ioeb_write(a, v);
                        }
                        else /*Internal latches*/
                        {
                                switch (a&0xFFFC)
                                {
                                        case 0x0000: case 0x0004: case 0x0008: case 0x000C:
                                        case 0x0028: case 0x002C:
                                        if (st506_present)
                                                st506_internal_writeb(a, v);
                                        return;
                                        case 0x0010: /*Printer*/
                                        printer_data_write(v);
                                        return;
                                        case 0x0018:
                                        wd1770_writelatch_b(v);
                                        return; /*FDC Latch B*/
                                        case 0x0040: /*FDC Latch A*/
                                        wd1770_writelatch_a(v);
                                        return;
                                        case 0x0048: /*Video clock (A540)*/
                                        vidc_setclock(v & 3);
                                        return;
                                }
                        }
                        break;
                        case 6: /*Backplane*/
                        switch (a&0xFFFC)
                        {
                                case 0x0000:
                                return;
                                case 0x0004:
                                podule_write_backplane_mask(v);
                                return;
                        }
                        break;
                }
                return;
        }
        LOG_DATABORT("Dat abort writeb %07X %07X %08X %i %i\n",a,PC, memstat[((a)>>12)&0x3FFF], modepritablew[memmode][memstat[((a)>>12)&0x3FFF]], modepritablew[memmode][memstat[((a)>>12)&0x3FFF]] && !((a)>>26));
        databort=1;
}

void writememfl(uint32_t a,uint32_t v)
{
        a &= 0x3ffffff;
/*        if (a==(0x1801010))
        {
                rpclog("Writel R12+284 %07X %08X %08X\n",PC,v,a);
                mempoint[((a)>>15)&0x7FF][((a)&0x7FFF)>>2]=v;
                return;
        }*/
/*        if (a==0x1803BEC)
        {
                rpclog("Write 1803BEC %02X %07X %i\n",v,PC,ins);
                mempoint[((a)>>15)&0x7FF][((a)&0x7FFF)>>2]=v;
                return;
        }*/
/*        if ((a<0x2000000) && modepritablew[memmode][memstat[((a)>>15)&0x7FF]])
        {
                sprintf(s,"Write %04X %08X %08X\n",a,v,PC);
                fputs(s,olog);
                mempoint[((a)>>15)&0x7FF][((a)&0x7FFF)>>2]=v;
//                output=1; timetolive=50;
                return;
        }*/
        switch (a>>20)
        {
#if 0
                case 0x00: case 0x01: case 0x02: case 0x03: /*RAM*/
                case 0x04: case 0x05: case 0x06: case 0x07:
                case 0x08: case 0x09: case 0x0A: case 0x0B:
                case 0x0C: case 0x0D: case 0x0E: case 0x0F:
                case 0x10: case 0x11: case 0x12: case 0x13:
                case 0x14: case 0x15: case 0x16: case 0x17:
                case 0x18: case 0x19: case 0x1A: case 0x1B:
                case 0x1C: case 0x1D: case 0x1E: case 0x1F:
                databort=1;
                return;
#endif

                case 0x30:
                case 0x31:
                case 0x32: /*IOC*/
                case 0x33:
                if (!(a & ((1 << 16) | (1 << 17) | (1 << 21)))) /*MEMC podule space*/
                {
                        podule_memc_write_w((a & 0xc000) >> 14, a & 0x3fff, v);
                        return;
                }
                if (fdctype == FDC_82C711 && a >= 0x3012000 && a <= 0x302A000)
                {
                        c82c711_fdc_dmawrite(a, v);
                        return;
		}
//                rpclog("IOC space writel %08X %02X\n",a,v);
                bank=(a>>16)&7;
                switch (bank)
                {
                        case 0: /*IOC*/
                        if (a & (1 << 21))
                                ioc_write(a,v>>16);
                        return;
                        case 1: /*1772 FDC*/
                        if (fdctype == FDC_WD1770)
                                wd1770_write(a,v>>16);
                        else
                        {
                                if ((a&0xFFF)==0x7C0)
                                {
                                        writeidew(&ide_internal, v >> 16);
                                        return;
                                }
                                c82c711_write(a,v);
                        }
                        return;
                        case 2: /*Econet*/
                        return;
                        case 3: /*Serial*/
                        return;
                        case 4: /*Internal podules*/
/*                        if (romset>3)
                        {
                                if ((a&~0x1F)==0x33c0004) { writeeterna(a,v); return; }
                                if ((a&~0x1F)==0x3340000) { writeeterna(a,v); return; }
                                if ((a&~0x1F)==0x33C0000) { writeeterna(a,v); return; }
                        }*/
                        podule_write_w((a & 0xC000) >> 14, a & 0x3FFF, v);
                        return;
                        case 5: /*IOEB*/
                        v>>=16;
                        if (fdctype == FDC_82C711)
                        {
                                ioeb_write(a, v);
                        }
                        else /*Internal latches*/
                        {
                                switch (a&0xFFFC)
                                {
                                        case 0x0000: case 0x0004: case 0x0008: case 0x000C:
                                        case 0x0028: case 0x002C:
                                        if (st506_present)
                                                st506_internal_writel(a, v);
                                        return;
                                        case 0x0010: /*Printer*/
                                        printer_data_write(v);
                                        return;
                                        case 0x0018:
                                        wd1770_writelatch_b(v);
                                        return; /*FDC Latch B*/
                                        case 0x0040: /*FDC Latch A*/
                                        wd1770_writelatch_a(v);
                                        return;
                                        case 0x0048: return; /*????*/
                                        case 0x0050: return; /*IOEB*/
                                        case 0x0074: return; /*????*/
                                        case 0x0078: return; /*????*/
//                                        default: rpclog("Bad 5 writel %08X\n",a);
                                }
                        }
                        break;
                        case 6: /*Backplane*/
                        switch (a&0xFFFC)
                        {
                                case 0x0000:
                                return;
                                case 0x0004:
                                podule_write_backplane_mask(v >> 16);
                                return;
                        }
                        break;
                }
                return;
                case 0x34: case 0x35: /*VIDC*/
		LOG_VIDC_REGISTERS("Write VIDC %08X %08X %07X %08X\n",a,v,PC,armregs[15]);
                writevidc(v);
                return;
                case 0x36: /*MEMC*/
                writememc(a);
                return;
                case 0x38: case 0x39: case 0x3A: case 0x3B: /*CAM*/
                case 0x3C: case 0x3D: case 0x3E: case 0x3F:
                writecam(a);
                return;
//                case 0x35: return; /*??? - Fire & Ice writes here*/
        }
        LOG_DATABORT("Dat abort writel %07X %07X\n",a,PC);
        databort=1;
//        rpclog("Dat abort writel %07X %07X\n",a,PC);
/*        sprintf(err2,"Bad write long %06X %03X %04X %08X\n",a,a>>15,a&0x7FFF,v);*/
}
       
