/*Arculator 0.8 by Tom Walker
  IDE emulation*/
#include <stdio.h>
#include "arc.h"
#include "ioc.h"

int dumpedread=0;
struct
{
        uint8_t atastat;
        uint8_t error,status;
        int secount,sector,cylinder,head,drive,cylprecomp;
        uint8_t command;
        uint8_t fdisk;
        int pos;
        int spt[2],hpc[2];
} ide;

int idereset=0;
uint16_t idebuffer[256];
uint8_t *idebufferb;
FILE *hdfile[2]={NULL,NULL};
void closeide()
{
        if (hdfile[0]) fclose(hdfile[0]);
        if (hdfile[1]) fclose(hdfile[1]);
}

int skip512[2]={0,0};
void resetide()
{
        int c;
        char fn[512];
        ide.drive=0;
        ide.atastat=0x40;
        idecallback=0;
	hdfile[0]=hdfile[1]=NULL;
	append_filename(fn,exname,"hd4.hdf",511);
        if (!hdfile[0])
        {
                hdfile[0]=fopen(fn,"rb+");
                if (!hdfile[0])
                {
                        hdfile[0]=fopen(fn,"wb");
                        putc(0,hdfile[0]);
                        fclose(hdfile[0]);
                        hdfile[0]=fopen(fn,"rb+");
                }
                atexit(closeide);
        }
	append_filename(fn,exname,"hd5.hdf",511);
        if (!hdfile[1])
        {
                hdfile[1]=fopen(fn,"rb+");
                if (!hdfile[1])
                {
                        hdfile[1]=fopen(fn,"wb");
                        putc(0,hdfile[1]);
                        fclose(hdfile[1]);
                        hdfile[1]=fopen(fn,"rb+");
                }
        }
        idebufferb=(uint8_t *)idebuffer;
        for (c=0;c<2;c++)
        {
                fseek(hdfile[c],0xFC1,SEEK_SET);
                ide.spt[c]=getc(hdfile[c]);
                ide.hpc[c]=getc(hdfile[c]);
                skip512[c]=1;
                if (!ide.spt[c] || !ide.hpc[c])
                {
                        fseek(hdfile[c],0xDC1,SEEK_SET);
                        ide.spt[c]=getc(hdfile[c]);
                        ide.hpc[c]=getc(hdfile[c]);
                        skip512[c]=0;
                        if (!ide.spt[c] || !ide.hpc[c])
                        {
                                ide.spt[c]=63;
                                ide.hpc[c]=16;
                                skip512[c]=1;
                        }
                }
//        rpclog("Drive %i - %i %i\n",c,ide.spt[c],ide.hpc[c]);
        }
//        ide.spt=63;
//        ide.hpc=16;
//        ide.spt=16;
//        ide.hpc=14;
}

void writeidew(uint16_t val)
{
//        if (ide.sector==7) rpclog("Write data %08X %04X\n",ide.pos,val);
        idebuffer[ide.pos>>1]=val;
        ide.pos+=2;
        if (ide.pos>=512)
        {
                ide.pos=0;
                ide.atastat=0x80;
                idecallback=200;
        }
}

void writeide(uint32_t addr, uint8_t val)
{
//        if (addr!=0x1F0) rpclog("Write IDE %08X %02X %08X %08X\n",addr,val,PC-8,armregs[12]);
        switch (addr)
        {
                case 0x1F0:
                idebufferb[ide.pos++]=val;
                if (ide.pos>=512)
                {
                        ide.pos=0;
                        ide.atastat=0x80;
                        idecallback=1000;
                }
                return;
                case 0x1F1:
                ide.cylprecomp=val;
                return;
                case 0x1F2:
                ide.secount=val;
                if (!val) ide.secount=256;
//                rpclog("secount %i %02X\n",ide.secount,val);
                return;
                case 0x1F3:
                ide.sector=val;
                return;
                case 0x1F4:
                ide.cylinder=(ide.cylinder&0xFF00)|val;
                return;
                case 0x1F5:
                ide.cylinder=(ide.cylinder&0xFF)|(val<<8);
                return;
                case 0x1F6:
                ide.head=val&0xF;
                ide.drive=(val>>4)&1;
//                rpclog("Write IDE head %02X %i,%i\n",val,ide.head,ide.drive);
                return;
                case 0x1F7: /*Command register*/
  //              rpclog("Starting new command %02X\n",val);
                ide.command=val;
                ide.error=0;
                switch (val)
                {
                        case 0x10: /*Restore*/
                        case 0x70: /*Seek*/
                        ide.atastat=0x40;
                        idecallback=100;
                        return;
                        case 0x20: /*Read sector*/
/*                        if (ide.secount>1)
                        {
                                error("Read %i sectors from sector %i cylinder %i head %i\n",ide.secount,ide.sector,ide.cylinder,ide.head);
                                exit(-1);
                        }*/
//                        rpclog("Read %i sectors from sector %i cylinder %i head %i\n",ide.secount,ide.sector,ide.cylinder,ide.head);
                        ide.atastat=0x80;
                        idecallback=100;
                        return;
                        case 0x30: /*Write sector*/
/*                        if (ide.secount>1)
                        {
                                error("Write %i sectors to sector %i cylinder %i head %i\n",ide.secount,ide.sector,ide.cylinder,ide.head);
                                exit(-1);
                        }*/
//                        rpclog("Write %i sectors to sector %i cylinder %i head %i\n",ide.secount,ide.sector,ide.cylinder,ide.head);
                        ide.atastat=0x08;
                        ide.pos=0;
                        return;
                        case 0x40: /*Read verify*/
                        case 0x41:
//                        rpclog("Read verify %i sectors from sector %i cylinder %i head %i\n",ide.secount,ide.sector,ide.cylinder,ide.head);
                        ide.atastat=0x80;
                        idecallback=200;
                        return;
                        case 0x50:
//                        rpclog("Format track %i head %i\n",ide.cylinder,ide.head);
                        ide.atastat=0x08;
//                        idecallback=200;
                        ide.pos=0;
                        return;
                        case 0x91: /*Set parameters*/
                        ide.atastat=0x80;
                        idecallback=200;
                        return;
                        case 0xA1: /*Identify packet device*/
                        case 0xE3: /*Idle*/
                        ide.atastat=0x80;
                        idecallback=200;
                        return;
                        case 0xEC: /*Identify device*/
                        ide.atastat=0x80;
                        idecallback=200;
                        return;
                        case 0xE5: /*Standby power check*/
                        ide.atastat=0x80;
                        idecallback=200;
                        return;
                }
                error("Bad IDE command %02X\n",val);
                exit(-1);
                return;
                case 0x3F6:
                if ((ide.fdisk&4) && !(val&4))
                {
                        idecallback=500;
                        idereset=1;
                        ide.atastat=0x80;
//                        rpclog("IDE Reset\n");
                }
                ide.fdisk=val;
                return;
        }
        error("Bad IDE write %04X %02X\n",addr,val);
        dumpregs();
        exit(-1);
}

uint8_t readide(uint32_t addr)
{
        uint8_t temp;
//        if (addr!=0x1F0) rpclog("Read IDE %08X %08X\n",addr,PC-8);
        switch (addr)
        {
                case 0x1F0:
//                rpclog("Read data %08X %02X\n",ide.pos,idebufferb[ide.pos]);
/*                if (!dumpedread)
                {
                        f=fopen("ram212.dmp","wb");
                        for (c=0x2127800;c<0x2127C00;c++)
                        {
                                putc(readmemb(c),f);
                        }
                        fclose(f);
                }*/
                temp=idebufferb[ide.pos];
                ide.pos++;
                if (ide.pos>=512)
                {
                        ide.pos=0;
                        ide.atastat=0x40;
                }
                return temp;
                case 0x1F1:
//                rpclog("Read IDEerror %02X\n",ide.atastat);
                return ide.error;
                case 0x1F2:
                return ide.secount;
                case 0x1F3:
                return ide.sector;
                case 0x1F4:
                return ide.cylinder&0xFF;
                case 0x1F5:
                return ide.cylinder>>8;
                case 0x1F6:
//                        rpclog("Read IDE Head Drive %02X %02X\n",ide.head|(ide.drive<<4),armregs[1]);
                return ide.head|(ide.drive<<4)|0xA0;
                case 0x1F7:
                if (romset==3)
                        ioc_irqbc(IOC_IRQB_IDE);
//                rpclog("Read ATAstat %02X\n",ide.atastat);
                return ide.atastat;
                case 0x3F6:
//                rpclog("Read ATAstat %02X\n",ide.atastat);
                return ide.atastat;
        }
        error("Bad IDE read %04X\n",addr);
        dumpregs();
        exit(-1);
}

uint16_t readidew()
{
        uint16_t temp;
//        if (ide.sector==7) rpclog("Read data2 %08X %04X\n",ide.pos,idebuffer[ide.pos>>1]);
        temp=idebuffer[ide.pos>>1];
//        rpclog("Read IDEW %04X\n",temp);
        ide.pos+=2;
        if (ide.pos>=512)
        {
                ide.pos=0;
                ide.atastat=0x40;
                if (ide.command==0x20)
                {
                        ide.secount--;
//                        rpclog("Sector done - secount %i\n",ide.secount);
                        if (ide.secount)
                        {
                                ide.atastat=0x08;
                                ide.sector++;
                                if (ide.sector==(ide.spt[ide.drive]+1))
                                {
                                        ide.sector=1;
                                        ide.head++;
                                        if (ide.head==(ide.hpc[ide.drive]))
                                        {
                                                ide.head=0;
                                                ide.cylinder++;
                                        }
                                }
                                ide.atastat=0x80;
                                idecallback=200;
                        }
                }
        }
        return temp;
}

void callbackide()
{
        int addr,c;
//        rpclog("IDE callback %08X %i %02X\n",hdfile[ide.drive],ide.drive,ide.command);
	if (!hdfile[ide.drive]) 
	{
                ide.atastat=0x41;
                ide.error=4;
                if (romset==3)
                        ioc_irqb(IOC_IRQB_IDE);
		return;
	}
        if (idereset)
        {
                ide.atastat=0x40;
                ide.error=0;
                ide.secount=1;
                ide.sector=1;
                ide.head=0;
                ide.cylinder=0;
                idereset=0;
//                rpclog("Reset callback\n");
                return;
        }
        switch (ide.command)
        {
                case 0x10: /*Restore*/
                case 0x70: /*Seek*/
//                rpclog("Restore callback\n");
                ide.atastat=0x40;
                if (romset==3)
                        ioc_irqb(IOC_IRQB_IDE);                
                return;
                case 0x20: /*Read sectors*/
                if (!ide.secount)
                {
                        ide.atastat=0x40;
                        return;
                }
                readflash[0]=1;
                addr=((((ide.cylinder*ide.hpc[ide.drive])+ide.head)*ide.spt[ide.drive])+(ide.sector))*512;
                if (!skip512[ide.drive]) addr-=512;
                /*                if (ide.cylinder || ide.head)
                {
                        error("Read from other cylinder/head");
                        exit(-1);
                }*/
//                rpclog("Seek to %08X\n",addr);
                fseek(hdfile[ide.drive],addr,SEEK_SET);
                fread(idebuffer,512,1,hdfile[ide.drive]);
                ide.pos=0;
                ide.atastat=0x08;
//                rpclog("Read sector callback %i %i %i offset %08X %i left %i\n",ide.sector,ide.cylinder,ide.head,addr,ide.secount,ide.spt[ide.drive]);
                if (romset==3)
                        ioc_irqb(IOC_IRQB_IDE);                
                return;
                case 0x30: /*Write sector*/
                readflash[0]=2;
                addr=((((ide.cylinder*ide.hpc[ide.drive])+ide.head)*ide.spt[ide.drive])+(ide.sector))*512;
                if (!skip512[ide.drive]) addr-=512;
//                rpclog("Write sector callback %i %i %i offset %08X %i left %i %i %i\n",ide.sector,ide.cylinder,ide.head,addr,ide.secount,ide.spt[ide.drive],ide.hpc[ide.drive],ide.drive);
                fseek(hdfile[ide.drive],addr,SEEK_SET);
                fwrite(idebuffer,512,1,hdfile[ide.drive]);
                if (romset==3)
                        ioc_irqb(IOC_IRQB_IDE);                
                ide.secount--;
                if (ide.secount)
                {
                        ide.atastat=0x08;
                        ide.pos=0;
                        ide.sector++;
                        if (ide.sector==(ide.spt[ide.drive]+1))
                        {
                                ide.sector=1;
                                ide.head++;
                                if (ide.head==(ide.hpc[ide.drive]))
                                {
                                        ide.head=0;
                                        ide.cylinder++;
                                }
                        }
                }
                else
                   ide.atastat=0x40;
                return;
                case 0x40: /*Read verify*/
                case 0x41:
                ide.pos=0;
                ide.atastat=0x40;
//                rpclog("Read verify callback %i %i %i offset %08X %i left\n",ide.sector,ide.cylinder,ide.head,addr,ide.secount);
                if (romset==3)
                        ioc_irqb(IOC_IRQB_IDE);                
                return;
                case 0x50: /*Format track*/
                addr=(((ide.cylinder*ide.hpc[ide.drive])+ide.head)*ide.spt[ide.drive])*512;
                if (!skip512[ide.drive]) addr-=512;
//                rpclog("Format cyl %i head %i offset %08X secount %I\n",ide.cylinder,ide.head,addr,ide.secount);
                fseek(hdfile[ide.drive],addr,SEEK_SET);
                memset(idebufferb,0,512);
                for (c=0;c<ide.secount;c++)
                {
                        fwrite(idebuffer,512,1,hdfile[ide.drive]);
                }
                ide.atastat=0x40;
                if (romset==3)
                        ioc_irqb(IOC_IRQB_IDE);                
                return;
                case 0x91: /*Set parameters*/
                ide.spt[ide.drive]=ide.secount;
                ide.hpc[ide.drive]=ide.head+1;
//                rpclog("%i sectors per track, %i heads per cylinder  %i %i  %i\n",ide.spt[ide.drive],ide.hpc[ide.drive],ide.secount,ide.head,ide.drive);
                ide.atastat=0x40;
                if (romset==3)
                        ioc_irqb(IOC_IRQB_IDE);                
                return;
                case 0xA1:
                case 0xE3:
                        case 0xE5:
                ide.atastat=0x41;
                ide.error=4;
                if (romset==3)
                        ioc_irqb(IOC_IRQB_IDE);
                return;
                case 0xEC:
//                        rpclog("Callback EC\n");
                memset(idebuffer,0,512);
                idebuffer[1]=101; /*Cylinders*/
                idebuffer[3]=16;  /*Heads*/
                idebuffer[6]=63;  /*Sectors*/
                for (addr=10;addr<20;addr++)
                    idebuffer[addr]=0x2020;
                for (addr=23;addr<47;addr++)
                    idebuffer[addr]=0x2020;
                idebufferb[46^1]='v'; /*Firmware version*/
                idebufferb[47^1]='0';
                idebufferb[48^1]='.';
                idebufferb[49^1]='5';
                idebufferb[54^1]='A'; /*Drive model*/
                idebufferb[55^1]='r';
                idebufferb[56^1]='c';
                idebufferb[57^1]='u';
                idebufferb[58^1]='l';
                idebufferb[59^1]='a';
                idebufferb[60^1]='t';
                idebufferb[61^1]='o';
                idebufferb[62^1]='r';
                idebufferb[63^1]='H';
                idebufferb[64^1]='D';
                idebuffer[50]=0x4000; /*Capabilities*/
                ide.pos=0;
                ide.atastat=0x08;
//                rpclog("ID callback\n");
                if (romset==3)
                        ioc_irqb(IOC_IRQB_IDE);                
                return;
        }
}
/*Read 1F1*/
/*Error &108A1 - parameters not recognised*/
