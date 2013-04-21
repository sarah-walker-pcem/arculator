/*Arculator 0.8 by Tom Walker
  Memory read/write functions*/
#include <allegro.h>
#include <winalleg.h>
#include <stdio.h>
#include "arc.h"

int bank;
int ddensity;
int prefabort;
int timetolive;
char err2[256];
FILE *olog;
int fdcside;
int realmemsize;
void initmem(int memsize)
{
        int c,d=(memsize>>2)-1;
        realmemsize=memsize;
        ram=(unsigned long *)malloc(memsize*1024);
        rom=(unsigned long *)malloc(0x200000);
        for (c=0;c<0x4000;c++) memstat[c]=0;
        for (c=0x2000;c<0x3000;c++) memstat[c]=3;
        for (c=0x2000;c<0x3000;c++) mempoint[c]=&ram[(c&d)<<10];
        for (c=0x3800;c<0x4000;c++) memstat[c]=5;
        for (c=0x3800;c<0x4000;c++) mempoint[c]=&rom[(c&0x1FF)<<10];
        memset(ram,0,memsize*1024);
        memstat[0]=1;
        mempoint[0]=rom;
        for (c=0;c<0x4000;c++) mempointb[c]=(unsigned char *)mempoint[c];
        realmemsize=memsize;
        initmemc();
}

void resizemem(int memsize) /*memsize is 4096,8192,16384*/
{
        int c,d=(memsize>>2)-1;
        free(ram);
        ram=(unsigned long *)malloc(memsize*1024);
        for (c=0x2000;c<0x3000;c++) mempoint[c]=&ram[(c&d)<<10];
        for (c=0x2000;c<0x3000;c++) mempointb[c]=(unsigned char *)mempoint[c];
        memset(ram,0,memsize*1024);
        realmemsize=memsize;
        initmemc();
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
                for (c=0x2000;c<0x3000;c++) mempointb[c]=(unsigned char *)mempoint[c];
        }
        else if (pagesize==3 && realmemsize==1024)
        {
                for (c=0x2000;c<0x3000;c++)
                {
                        e=c&511;
                        e=(e&1)|((e&~3)<<1);
                        mempoint[c]=&ram[((e&0x1FF)<<10)];
                }
                for (c=0x2000;c<0x3000;c++) mempointb[c]=(unsigned char *)mempoint[c];
        }
        else if (pagesize==3 && realmemsize==512)
        {
                for (c=0x2000;c<0x3000;c++)
                {
                        e=c&255;
                        e=(e&1)|((e&~3)<<1);
                        mempoint[c]=&ram[((e&0xFF)<<10)];
                }
                for (c=0x2000;c<0x3000;c++) mempointb[c]=(unsigned char *)mempoint[c];
        }
        else
        {
                for (c=0x2000;c<0x3000;c++) mempoint[c]=&ram[(c&d)<<10];
                for (c=0x2000;c<0x3000;c++) mempointb[c]=(unsigned char *)mempoint[c];
        }
}

unsigned long readmemf(unsigned long a)
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
                prefabort=1;
                return 0;
                case 0x3F:
                        return 0xFFFFFFFF;
//#endif
        }
        prefabort=1;
        rpclog("Prefabort %08X\n",a);
        return 0xdeadbeef;
/*        sprintf(err2,"Bad fetch %06X %03X %04X\n",a,a>>15,a&0x7FFF);
        MessageBox(NULL,err2,"Arc",MB_OK);
        dumpregs();
        exit(-1);*/
}

unsigned char readmemfb(unsigned long a)
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
        if (a&0xFC000000) { rpclog("Databort readmemfb %08X\n",a); databort=2; return 0xef; }
/*        if ((a&~0xFFF)==0x8000)
        {
                printf("Read %04X %08X %02X\n",a,PC,mempointb[((a)>>15)&0x7FF][(a)&0x7FFF]);
                return mempointb[((a)>>15)&0x7FF][(a)&0x7FFF];
        }*/
        switch (a>>20)
        {
                case 0x30: /*82c711*/
//                rpclog("Readb %08X %07X\n",a,PC);
                if (a>=0x3012000 && a<=0x302A000)
                   return readfdcdma(a);
                return read82c711(a);
//                rpclog("Read 82c711 %08X %02X\n",a,temp);
//                return temp;
                case 0x32: /*IOC*/
                case 0x33:
                bank=(a>>16)&7;
                switch (bank)
                {
                        case 0: /*IOC*/
                        return readioc(a);
                        case 1: /*1772 FDC*/
                        if (romset<3) return read1772(a);
                        return 0xFF;
                        case 2: /*Econet*/
                        return 0xFF;
                        case 3: /*Serial*/
                        return 0xFF;
                        case 4: /*Internal podules*/
                        if (romset>3)
                        {
                                if ((a&~0x1F)==0x33c0004) return readeterna(a);
                                if ((a&~0x1F)==0x3340000) return readeterna(a);
                                if ((a&~0x1F)==0x33C0000) return readeterna(a);
                        }
                        if (romset<3 && !(a&0xC000)) /*ICS interface lives in slot 0*/
                        {
//                                printf("Read ICS B %08X %07X\n",a,PC);
                                if ((a&0x3FFF)<0x2800) return readics(a);
                                if ((a&0x3FFF)<0x3020) { /*rpclog("Read IDE %08X\n",a);*/ return readide(((a>>2)&7)+0x1F0); }
                        }
                        if ((a&0xC000)==0x4000) /*Extension ROMs in slot 1*/
                           return readarcrom(a);
                        return 0xFF;
                        case 5: /*Internal latches*/
                        switch (a&0xFFFC)
                        {
                                case 0x0010: return 0xFF; /*Printer*/
                                case 0x0018:
                                return 0xFF; /*FDC Latch B*/
                                case 0x0040: /*FDC Latch A*/
                                return 0xFF;
                                case 0x0048: return 0xFF; /*????*/
                                case 0x0050: return (fdctype)?5:0; /*IOEB*/
                                case 0x0070: return 0xF;
                                case 0x0074: return 0xFF; /*????*/
                                case 0x0078: case 0x7C: return readjoy(a); /*Joystick (A3010)*/
                                default:
                                return readst506(a);
                        }
                }
                return 0xFF;
        }
//        rpclog("Data abort b %07X\n",a);
        databort=1;
//        rpclog("Dat abort readb %07X %07X\n",a,PC);
        return 0xef;
/*        sprintf(err2,"Bad read byte %06X %03X %04X\n",a,a>>15,a&0x7FFF);
        MessageBox(NULL,err2,"Arc",MB_OK);
        dumpregs();
        exit(-1);*/
}

unsigned long readmemfl(unsigned long a)
{
//        unsigned long temp;
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
        if (a&0xFC000000) { rpclog("Databort readmemfl %08X\n",a); databort=2; return 0xdeadbeef; }
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
                case 0x30: /*82c711*/
//                rpclog("Readl %08X %07X\n",a,PC);
                if (a>=0x3012000 && a<=0x302A000)
                   return readfdcdma(a);
                if ((a&0xFFF)==0x7C0) return readidew();
                return read82c711(a);
//                rpclog("Readl 82c711 %08X %02X\n",a,temp);
//                return temp;
                case 0x32: /*IOC*/
                case 0x33:
                bank=(a>>16)&7;
                switch (bank)
                {
                        case 0: /*IOC*/
                        return readioc(a);
                        case 1: /*1772 FDC*/
                        if (romset<3) return read1772(a);
                        return 0xFFFF;
                        case 2: /*Econet*/
                        return 0xFFFF;
                        case 3: /*Serial*/
                        return 0xFFFF;
                        case 4: /*Internal podules*/
                        if (romset>3)
                        {
                                if ((a&~0x1F)==0x33c0004) return readeterna(a);
                                if ((a&~0x1F)==0x3340000) return readeterna(a);
                                if ((a&~0x1F)==0x33C0000) return readeterna(a);
                        }
                        if (romset<3 && !(a&0xC000)) /*ICS interface lives in slot 0*/
                        {
//                                printf("Read ICS W %08X\n",a);
                                if ((a&0x3FFF)==0x2800) return readidew(); 
                                if ((a&0x3FFF)<0x2800) return readics(a);
                                if ((a&0x3FFF)<0x3020) return readide(((a>>2)&7)+0x1F0);

                        }
                        if ((a&0xC000)==0x4000) /*Extension ROMs in slot 1*/
                           return readarcrom(a);
                        return 0xFFFF;
                        case 5: /*Internal latches*/
                        switch (a&0xFFFC)
                        {
                                case 0x0010: return 0xFFFF; /*Printer*/
                                case 0x0018:
                                return 0xFFFF; /*FDC Latch B*/
                                case 0x0040: /*FDC Latch A*/
                                return 0xFFFF;
                                case 0x0048: return 0xFFFF; /*????*/
                                case 0x0050: return (fdctype)?5:0; /*IOEB*/
                                case 0x0074: return 0xFFFF; /*????*/
                                case 0x0078: return 0xFFFF; /*????*/
                                default:
                                return readst506l(a);
                                return;
                        }
                }
                return 0xFFFF;
                case 0x34: case 0x35: case 0x36: case 0x37: /*Expansion ROMs*/
                return 0xFFFFFFFF;
        }
//        rpclog("Data abort l %07X\n",a);
        databort=1;
//        rpclog("Dat abort readl %07X %07X\n",a,PC);
        return 0xdeadbeef;
/*        sprintf(err2,"Bad read long %06X %03X %04X\n",a,a>>15,a&0x7FFF);
        MessageBox(NULL,err2,"Arc",MB_OK);
        dumpregs();
        exit(-1);*/
}

int f42count=0;
FILE *slogfile;
void writememfb(unsigned long a,unsigned char v)
{
        int bank;
        if (a&0xFC000000) { /*rpclog("Databort writememfb %08X\n",a);*/ databort=2; return; }
/*        if (a==0x1800F42)
        {
                rpclog("Write 1800F42 %02X %07X %i %08X %i\n",v,PC,ins,armregs[5],f42count);
                mempointb[((a)>>15)&0x7FF][(a)&0x7FFF]=v;
                output=0;
                if (f42count==7) output=1;
                f42count++;
                ins=0;
                return;
        }*/
/*        if ((a&~3)==0x1803BEC)
        {
                rpclog("Write %08X %07X %02X %08X\n",a,PC,v,a);
                mempointb[((a)>>15)&0x7FF][(a)&0x7FFF]=v;
                return;
        }*/
/*        if ((a<0x2000000) && modepritablew[memmode][memstat[((a)>>15)&0x7FF]])
        {
                if (!olog) olog=fopen("olog.txt","wt");
                sprintf(s,"Write %04X %02X %08X\n",a,v,PC);
                fputs(s,olog);
                mempointb[((a)>>15)&0x7FF][(a)&0x7FFF]=v;
//                output=1; timetolive=2500;
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
//                case 0x1F: return; if (a>=0x1f08000 && a<=0x1f0ffff) return; break;
                case 0x30: /*82c711*/
//                rpclog("Write %08X %02X %07X\n",a,v,PC);
                if (a>=0x3012000 && a<=0x302A000)
                {
                        writefdcdma(a,v);
                        return;
                }
                write82c711(a,v);
                return;
                case 0x32: /*IOC*/
                case 0x33:
                bank=(a>>16)&7;
                switch (bank)
                {
                        case 0: /*IOC*/
                        writeioc(a,v);
                        return;
                        case 1: /*1772 FDC*/
                        if (romset<3) write1772(a,v);
                        return;
                        case 2: /*Econet*/
                        return;
                        case 3: /*Serial*/
                        return;
                        case 4: /*Internal podules*/
                        if (romset>3)
                        {
                                if ((a&~0x1F)==0x33c0004) { writeeterna(a,v); return; }
                                if ((a&~0x1F)==0x3340000) { writeeterna(a,v); return; }
                                if ((a&~0x1F)==0x33C0000) { writeeterna(a,v); return; }
                        }
                        if (romset<3 && !(a&0xC000)) /*ICS interface lives in slot 0*/
                        {
//                                printf("Write ICS B %08X %02X\n",a,v);
                                if ((a&0x3FFF)<0x2800) { writeics(a,v); return; }
                                if ((a&0x3FFF)<0x3020) { /*rpclog("Write IDE %08X %02X\n",a,v); */writeide(((a>>2)&7)+0x1F0,v); return; }
                        }
                        if ((a&0xC000)==0x4000) /*Extension ROMs in slot 1*/
                        { writearcrom(a,v); return; }
                        return;
                        case 5: /*Internal latches*/
                        switch (a&0xFFFC)
                        {
                                case 0x0010: return; /*Printer*/
                                case 0x0018:
                                ddensity=!(v&2);
                                return; /*FDC Latch B*/
                                case 0x0040: /*FDC Latch A*/
                                fdcside=(v&0x10)?0:1;
                                if (!(v&1)) curdrive=0;
                                if (!(v&2)) curdrive=1;
                                if (!(v&4)) curdrive=2;
                                if (!(v&8)) curdrive=3;
                                motoron=!(v&0x20);
                                updateirqs();
                                return;
                                case 0x0048: return; /*????*/
                                case 0x0050: return; /*IOEB*/
                                case 0x0074: return; /*????*/
                                case 0x0078: return; /*????*/
                                default:
                                writest506(a,v);
                                return;
                        }
                }
                return;
        }
//        rpclog("Dat abort writeb %07X %07X\n",a,PC);
        databort=1;
}

void writememfl(unsigned long a,unsigned long v)
{
        if (a&0xFC000000) { /*rpclog("Databort writememfl %08X %07X\n",a,PC); */databort=2; return; }
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
                case 0x30: /*82c711*/
//                rpclog("Write %08X %08X %07X\n",a,v,PC);
                if (a>=0x3012000 && a<=0x302A000)
                {
                        writefdcdma(a,v);
                        return;
                }
                if ((a&0xFFF)==0x7C0)
                {
                        writeidew(v>>16);
                        return;
                }
                write82c711(a,v);
                return;
                case 0x32: /*IOC*/
                case 0x33:
                bank=(a>>16)&7;
                switch (bank)
                {
                        case 0: /*IOC*/
                        writeioc(a,v>>16);
                        return;
                        case 1: /*1772 FDC*/
                        if (romset<3) write1772(a,v>>16);
                        return;
                        case 2: /*Econet*/
                        return;
                        case 3: /*Serial*/
                        return;
                        case 4: /*Internal podules*/
                        if (romset>3)
                        {
                                if ((a&~0x1F)==0x33c0004) { writeeterna(a,v); return; }
                                if ((a&~0x1F)==0x3340000) { writeeterna(a,v); return; }
                                if ((a&~0x1F)==0x33C0000) { writeeterna(a,v); return; }
                        }
                        if (romset<3 && !(a&0xC000)) /*ICS interface lives in slot 0*/
                        {
//                                printf("Write ICS W %08X %04X\n",a,v>>16);
                                if ((a&0x3FFF)==0x2800) { writeidew(v>>16); return; }
                                if ((a&0x3FFF)<0x2800) { writeics(a,v); return; }
                                if ((a&0x3FFF)<0x3020) { /*rpclog("Write IDE %08X %02X\n",a,v); */writeide(((a>>2)&7)+0x1F0,v); return; }
                        }
                        if ((a&0xC000)==0x4000) /*Extension ROMs in slot 1*/
                        { writearcrom(a,v); return; }
                        return;
                        case 5: /*Internal latches*/
                        v>>=16;
                        switch (a&0xFFFC)
                        {
                                case 0x0010: return; /*Printer*/
                                case 0x0018:
                                ddensity=!(v&2);
                                return; /*FDC Latch B*/
                                case 0x0040: /*FDC Latch A*/
                                fdcside=(v&0x10)?0:1;
                                if (!(v&1)) curdrive=0;
                                if (!(v&2)) curdrive=1;
                                if (!(v&4)) curdrive=2;
                                if (!(v&8)) curdrive=3;
                                updateirqs();
                                motoron=!(v&0x20);
                                return;
                                case 0x0048: return; /*????*/
                                case 0x0050: return; /*IOEB*/
                                case 0x0074: return; /*????*/
                                case 0x0078: return; /*????*/
                                default:
                                writest506l(a,v);
                                return;
                        }
                }
                return;
                case 0x34: case 0x35: /*VIDC*/
//                printf("Write VIDC %08X %08X %07X %08X\n",a,v,PC,armregs[15]);
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
//        rpclog("Dat abort writel %07X %07X\n",a,PC);
        databort=1;
//        rpclog("Dat abort writel %07X %07X\n",a,PC);
/*        sprintf(err2,"Bad write long %06X %03X %04X %08X\n",a,a>>15,a&0x7FFF,v);*/
}
       
