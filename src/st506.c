#include <stdio.h>
#include "arc.h"
#include "ioc.h"

void callbackst506();
int printnext=0;
int timetolive;
#define BUSY            0x80
#define PARAMREJECT     0x40
#define COMEND          0x20
#define SEEKEND         0x10
#define DRIVEERROR      0x08
#define ABNEND          0x04

FILE *shdfile[2];
int idecallback;

struct
{
        uint8_t status;
        uint8_t param[16];
        uint8_t cul;
        uint8_t command;
        int p,wp;
        int track[2];
        int lsect,lcyl,lhead,drive;
        int oplen;
        uint8_t OM1;
        uint8_t ssb;
        int drq;
        int first;
} st506;
uint8_t st506buffer[272];

void st506_updateinterrupts()
{
        if ((st506.status & ~st506.OM1 & 0x38) || st506.drq)
                ioc_irqb(IOC_IRQB_ST506);
        else
                ioc_irqbc(IOC_IRQB_ST506);
        rpclog("ST506 status %i  %02X %02X %i\n",ioc.irqb&8,st506.status,st506.OM1,st506.drq);
//        if (ioc.irqb&8 && !oldirq) rpclog("HDC IRQ\n");
}
void resetst506()
{
        char fn[512];
//        return;
        st506.status=0;
        st506.p=st506.wp=0;
        st506.drq=0;
        st506.first=0;
//        if (shdfile[0]) fclose(shdfile[0]);
	append_filename(fn,exname,"HardImage1",511);
        shdfile[0]=fopen(fn,"rb+");
        if (!shdfile[0])
        {
                shdfile[0]=fopen(fn,"wb");
                putc(0,shdfile[0]);
                fclose(shdfile[0]);
                shdfile[0]=fopen(fn,"rb+");
        }
//        if (shdfile[1]) fclose(shdfile[1]);
	append_filename(fn,exname,"HardImage2",511);
        shdfile[1]=fopen(fn,"rb+");
        if (!shdfile[1])
        {
                shdfile[1]=fopen(fn,"wb");
                putc(0,shdfile[1]);
                fclose(shdfile[1]);
                shdfile[1]=fopen(fn,"rb+");
        }
}

#define TOV 0x58
#define IPH 0x3C
#define NSC 0x24
#define NRY 0x20

void st506error(uint8_t err)
{
//        rpclog("ST506 error - %02X\n",err);
        st506.status=ABNEND|COMEND;
        st506.drq=0;
        st506_updateinterrupts();
        st506.ssb=err;
}

void readdataerror()
{
        int c;
        for (c=9;c>=0;c--)
                st506.param[c+2]=st506.param[c];
        st506.param[0]=st506.param[1]=0;
        st506.p=st506.wp=0;
        st506.param[1]=st506.ssb;
}

int st506writes=0;
void writest506(uint32_t a, uint8_t v)
{
//        return;
        rpclog("Write HDC %08X %08X\n",a,v);
        error("ST506 write %08X %02X %02X at %07X\n",a,a&0x3C,v,PC);
        exit(-1);
}

uint8_t readst506(uint32_t a)
{
        uint16_t temp;
//        return 0xFF;
//        st506writes++;
//        return 0xFF;
        switch (a&0x3C)
        {
                case 0x24: /*Data read*/
                if (st506.p<16)
                {
                        temp=st506.param[st506.p++]<<8;
                        temp|=st506.param[st506.p++];
//                        st506.p+=2;
//                        rpclog("Reading params - returning %04X\n",temp);
//                        rpclog("Read HDC %08X %08X %02X\n",a,temp,ioc.irqb);
                        return temp;//st506.param[st506.p-1];
                }
                else if (st506.p<272)
                {
                        temp=st506buffer[st506.p++]<<8;
                        temp|=st506buffer[st506.p++];
                        if (st506.p==272)
                        {
                                st506.drq=0;
                                st506_updateinterrupts();
                                idecallback=40;
                        }
//                        rpclog("Read HDC %08X %08X %07X %02X\n",a,temp,PC,ioc.irqb);
                        return temp;
                }
                else
                {
                        st506.drq=0;
                        st506_updateinterrupts();
//                        rpclog("Read HDC %08X %08X %02X\n",a,0x23,ioc.irqb);
                        return 0x23;
                }
                break;
        }
        error("ST506 read %08X %02X at %07X\n",a,a&0x3C,PC);
        exit(-1);
}

void writest506l(uint32_t a, uint32_t v)
{
        uint8_t temp;
//        output=0;
//        return;
//        v>>=16;
//        rpclog("Write HDC %08X %08X %07X %02X\n",a&0x3C,v,PC,ioc.irqb);
        st506writes++;
        switch (a&0x3C)
        {
                case 0: /*New command*/
//                output=0;
//                rpclog("New ST506 command %04X\n",v);
                if (st506.status&BUSY && v!=0xF0)
                {
                        rpclog("Command rejected\n");
                        return;
                }
                st506.drq=0;
                if (v!=0xF0) st506.status=PARAMREJECT;
                st506.wp=st506.p=0;
                st506.command=v;
                if (v!=8 && v!=0xF0) st506.ssb=0;
                switch (v)
                {
                        case 0:
                                return;
                        case 0x08: /*Recall*/
                        st506.status=0;
                        st506_updateinterrupts();
                        return;
                        case 0x10: /*Enable polling*/
                        st506.status&=~PARAMREJECT;
                        return;
                        case 0x18: /*Disable polling*/
                        st506.status&=~PARAMREJECT;
                        return;
                        case 0x28: /*Check drive*/
                        if (st506.param[0]!=1 && st506.param[0]!=2) { st506error(NRY); readdataerror(); return; }
                        st506.drive=st506.param[0]-1;
                        temp=st506.param[0]&3;
                        st506.status=0;
                        st506.param[0]=0;
                        st506.param[1]=0;
                        st506.param[2]=temp;
                        st506.param[3]=0;
                        st506.param[4]=(st506.track[st506.drive])?0xC0:0xE0;
                        st506.param[5]=0;
                        return;
                        case 0x40: /*Read data*/
                        rpclog("Read data %02X %02X\n",st506.param[0],st506.param[1]);
                        if (st506.param[0]!=1 && st506.param[0]!=2) { st506error(NRY); readdataerror(); return; }
                        st506.drive=st506.param[0]-1;
                        st506.lcyl=(st506.param[2]<<8)|st506.param[3];
                        st506.lhead=st506.param[4];
                        st506.lsect=st506.param[5];
                        st506.oplen=(st506.param[6]<<8)|st506.param[7];
                        rpclog("Read data : cylinder %i head %i sector %i   length %i sectors\n",st506.lcyl,st506.lhead,st506.lsect,st506.oplen);
                        if (st506.lcyl>1023) { st506error(NSC); readdataerror(); return; }
                        if (st506.lhead>7)   { st506error(IPH); readdataerror(); return; }
                        if (st506.lsect>31)  { st506error(TOV); readdataerror(); return; }
                        fseek(shdfile[st506.drive],(((((st506.lcyl*8)+st506.lhead)*32)+st506.lsect)*256),SEEK_SET);
//                        rpclog("Seeked to %08X\n",(((((st506.lcyl*8)+st506.lhead)*32)+st506.lsect)*256));
                        idecallback=100;
                        st506.status|=0x80;
                        return;
                        case 0x48: /*Check data*/
                        if (st506.param[0]!=1 && st506.param[0]!=2) { st506error(NRY); readdataerror(); return; }
                        st506.drive=st506.param[0]-1;
                        st506.lcyl=(st506.param[2]<<8)|st506.param[3];
                        st506.lhead=st506.param[4];
                        st506.lsect=st506.param[5];
                        st506.oplen=(st506.param[6]<<8)|st506.param[7];
                        rpclog("Check data : cylinder %i head %i sector %i   length %i sectors\n",st506.lcyl,st506.lhead,st506.lsect,st506.oplen);
                        if (st506.lcyl>1023) { st506error(NSC); readdataerror(); return; }
                        if (st506.lhead>7)   { st506error(IPH); readdataerror(); return; }
                        if (st506.lsect>31)  { st506error(TOV); readdataerror(); return; }
                        fseek(shdfile[st506.drive],(((((st506.lcyl*8)+st506.lhead)*32)+st506.lsect)*256),SEEK_SET);
                        idecallback=100;
                        st506.status|=0x80;
                        return;
                        case 0x87: /*Write data*/
                        if (st506.param[0]!=1 && st506.param[0]!=2) { st506error(NRY); readdataerror(); return; }
                        st506.drive=st506.param[0]-1;
                        st506.lcyl=(st506.param[2]<<8)|st506.param[3];
                        st506.lhead=st506.param[4];
                        st506.lsect=st506.param[5];
                        st506.oplen=(st506.param[6]<<8)|st506.param[7];
                        rpclog("Write data : cylinder %i head %i sector %i   length %i sectors\n",st506.lcyl,st506.lhead,st506.lsect,st506.oplen);
                        if (st506.lcyl>1023) { st506error(NSC); readdataerror(); return; }
                        if (st506.lhead>7)   { st506error(IPH); readdataerror(); return; }
                        if (st506.lsect>31)  { st506error(TOV); readdataerror(); return; }
                        fseek(shdfile[st506.drive],(((((st506.lcyl*8)+st506.lhead)*32)+st506.lsect)*256),SEEK_SET);
//                        rpclog("Seeked to %08X\n",(((((st506.lcyl*8)+st506.lhead)*32)+st506.lsect)*256));
                        idecallback=100;
                        st506.status|=0x80;
                        st506.first=1;
                        return;
                        case 0xA3: /*Write Format*/
                        if (st506.param[0]!=1 && st506.param[0]!=2) { st506error(NRY); readdataerror(); return; }
                        st506.drive=st506.param[0]-1;
                        st506.lcyl =st506.track[st506.drive];
                        st506.lhead=st506.param[1];
                        st506.lsect=0;
                        st506.oplen=(st506.param[2]<<8)|st506.param[3];
                        rpclog("Write format : drive %i cylinder %i head %i sector %i   length %i sectors\n",st506.drive,st506.lcyl,st506.lhead,st506.lsect,st506.oplen);
                        if (st506.lcyl>1023) { st506error(NSC); readdataerror(); return; }
                        if (st506.lhead>7)   { st506error(IPH); readdataerror(); return; }
                        if (st506.lsect>31)  { st506error(TOV); readdataerror(); return; }
                        fseek(shdfile[st506.drive],(((((st506.lcyl*8)+st506.lhead)*32)+st506.lsect)*256),SEEK_SET);
                        idecallback=100;
                        st506.status|=0x80;
                        st506.first=1;
                        return;
                        case 0xC0: /*Seek*/
                        if (st506.param[0]!=1 && st506.param[0]!=2) { st506error(NRY); readdataerror(); return; }
                        st506.drive=st506.param[0]-1;
                        st506.track[st506.drive]=st506.param[3]|(st506.param[2]<<8);
                        rpclog("Seek drive %i to track %i\n",st506.drive, st506.track);
                        st506.param[0]=0;
                        st506.param[1]=0;
                        st506.param[2]=0;
                        st506.param[3]=st506.cul;
                        st506.status|=COMEND;
                        st506.status|=SEEKEND;
                        st506_updateinterrupts();
                        return;
                        case 0xC8: /*Recalibrate*/
                        if (st506.param[0]!=1 && st506.param[0]!=2) { st506error(NRY); readdataerror(); return; }
                        st506.drive=st506.param[0]-1;
                        st506.track[st506.drive]=0;
//                        rpclog("Recalibrate : seek to track %i\n",st506.track);
                        st506.status|=SEEKEND;
                        st506.param[0]=0;
                        st506.param[1]=0;
                        st506.param[2]=0;
                        st506.param[3]=st506.cul;
                        return;
                        case 0xE8: /*Specify*/
//                        rpclog("Specify\nOM1 = %02X\nSHRL = %02X\nSectors = %i\nHeads = %i\nCylinders = %i\n",st506.param[1],st506.param[8],st506.param[7]+1,st506.param[6]+1,(st506.param[5]|((st506.param[4]&3)<<8))+1);
                        st506.status=PARAMREJECT;
                        st506.OM1=st506.param[1];
                        st506.cul=st506.param[3];
                        st506.param[0]=0;
                        st506.param[1]=0;
//                        rpclog("OM1=%02X\n",st506.OM1);
                        return;
                        case 0xF0: /*Abort*/
                        st506.status=(st506.status&PARAMREJECT)|COMEND;
                        st506.param[0]=0;
                        st506.param[1]=4;
                        st506_updateinterrupts();
                        break;
                        case 0xFF: break;
                        default:
                        error("Bad ST506 command %02X\n",v);
                        exit(-1);
                }
                return;
                case 0x04: /*Params*/
                if (st506.wp<16)
                {
                        st506.param[st506.wp]=v>>8;
                        st506.param[st506.wp+1]=v;
//                        rpclog("Writing params - pointer %02X param %02X%02X\n",st506.wp,v>>8,v&0xFF);
                        st506.wp+=2;
                        if (st506.wp>=16) st506.status|=PARAMREJECT;
                }
                return;
                case 0x28: case 0x2C: /*DMA write*/
//                rpclog("Write DMA %i\n",st506.p);
                if (st506.p>=16 && st506.p<272)
                {
                        st506buffer[st506.p++]=v>>8;
                        st506buffer[st506.p++]=v;
                        if (st506.p==272)
                        {
                                st506.drq=0;
                                st506_updateinterrupts();
                                idecallback=20;
                        }
//                        rpclog("Write HDC %08X %08X %i %07X %i %02X  %02X %02X\n",a,temp,st506.p,PC,st506.drq,ioc.irqb,st506.status,st506.OM1);
                        return;
                }
                else if (st506.p>16)
                {
                        st506.drq=0;
                        st506_updateinterrupts();
//                        rpclog("Write HDC %08X %08X   %07X\n",a,1223,PC);
                }
                return;
        }
        error("ST506 writel %08X %02X %08X at %07X\n",a,a&0x3C,v,PC);
        exit(-1);
}

uint32_t readst506l(uint32_t a)
{
        uint16_t temp;
/*        if (printnext)
        {
                printnext=0;
                rpclog("Read %i\n",idecallback);
        }*/
//        st506writes++;
//        return 0xFFFF;
        switch (a&0x3C)
        {
                case 0x08: case 0x0C: /*DMA read*/
//                rpclog("Read DMA %i\n",st506.p);
                if (st506.p>=16 && st506.p<272)
                {
                        temp=st506buffer[st506.p++]<<8;
                        temp|=st506buffer[st506.p++];
                        if (st506.p==272)
                        {
                                st506.drq=0;
                                st506_updateinterrupts();
                                idecallback=20;
                        }
//                        rpclog("Read HDC %08X %08X %i %07X\n",a,temp,st506.p,PC);
                        return temp;
                }
                else if (st506.p>16)
                {
                        st506.drq=0;
                        st506_updateinterrupts();
//                        rpclog("Read HDC %08X %08X   %07X\n",a,1223,PC);
                        return 0x1223;
                }
                return 0;
                case 0x20:
//                rpclog("Return ST506 status %04X %07X %02X %02X\n",st506.status<<8,PC,ioc.irqb,ioc.mskb);
//                output=1;
//                timetolive=6;
//                rpclog("Read HDC %08X %08X %07X\n",a,st506.status<<8,PC);
                return st506.status<<8;
                case 0x24: /*Params*/
                if (st506.p<16)
                {
                        temp=st506.param[st506.p++]<<8;
                        temp|=st506.param[st506.p++];
//                        st506.p+=2;
//                        rpclog("Reading params - returning %04X\n",temp);
//                        rpclog("Read HDC %08X %08X %02X\n",a,temp,ioc.irqb);
                        return temp;//st506.param[st506.p-1];
                }
                return 0xFF;
        }
        error("ST506 readl %08X %02X at %07X\n",a,a&0x3C,PC);
        exit(-1);
}

void dumpst506buffer()
{
        int c;
        for (c=0;c<256;c++)
        {
                printf("%02X ",st506buffer[c+16]);
                if ((c&15)==15) printf("\n");
        }
}

void callbackst506()
{
        int c;
        uint8_t temp;
//        return;
//        rpclog("Callback!\n");
        switch (st506.command)
        {
                case 0x40: /*Read sector*/
                if (st506.oplen)
                {
//                        if (st506.lsect>31)  { st506error(TOV); readdataerror(); return; }
                        st506.lsect++;
                        if (st506.lsect==32)
                        {
                                st506.lsect=0;
                                st506.lhead++;
                                if (st506.lhead==8)
                                {
                                        st506.lhead=0;
                                        st506.lcyl++;
                                        if (st506.lcyl>1023)
                                        {
                                                error("Hit limit\n");
                                                exit(-1);
                                        }
                                }
                        }
//                        rpclog("Reading from pos %08X - %i sectors left\n",ftell(hdfile),st506.oplen);
                        st506.oplen--;
//                        rpclog("Read ST506buffer from %08X\n",ftell(hdfile));
                        fread(st506buffer+16,256,1,shdfile[st506.drive]);
//                        if ((ftell(hdfile)-256)==0x2048C00) dumpst506buffer();
                        for (c=16;c<272;c+=2)
                        {
                                temp=st506buffer[c];
                                st506buffer[c]=st506buffer[c+1];
                                st506buffer[c+1]=temp;
                        }
                        st506.p=16;
                        st506.drq=1;
                        st506_updateinterrupts();
//                        rpclog("HDC interrupt part\n");
                }
                else
                {
                        for (c=9;c>=0;c--)
                            st506.param[c+2]=st506.param[c];
                        st506.param[0]=st506.param[1]=0;
                        st506.p=st506.wp=0;
//                        rpclog("Finished read sector! %02X\n",st506.OM1);
                        st506.status|=COMEND|PARAMREJECT;
                        st506.status&=~0x80;
                        st506.drq=0;
                        st506_updateinterrupts();
//                        rpclog("HDC interrupt full\n");
                }
                break;
                case 0x48: /*Check data*/
//                rpclog("Check data %i\n",st506.oplen);
                if (st506.oplen)
                {
                        st506.lsect++;
                        if (st506.lsect==32)
                        {
                                st506.lsect=0;
                                st506.lhead++;
                                if (st506.lhead==8)
                                {
                                        st506.lhead=0;
                                        st506.lcyl++;
                                        if (st506.lcyl>1023)
                                        {
                                                error("Hit limit\n");
                                                exit(-1);
                                        }
                                }
                        }
                        st506.oplen--;
                        idecallback=100;
//                        rpclog("Check data next callback\n");
                }
                else
                {
                        for (c=9;c>=0;c--)
                            st506.param[c+2]=st506.param[c];
                        st506.param[0]=st506.param[1]=0;
                        st506.p=st506.wp=0;
                        st506.status|=COMEND|PARAMREJECT;
                        st506.status&=~0x80;
                        st506.drq=0;
                        st506_updateinterrupts();
//                        rpclog("Check data over\n");
                }
                break;
                case 0x87: /*Write sector*/
                if (st506.first)
                {
                        st506.p=16;
                        st506.drq=1;
                        st506_updateinterrupts();
//                        rpclog("Write HDC interrupt first\n");
                        st506.first=0;
                }

                else
                {
                        st506.lsect++;
                        if (st506.lsect==32)
                        {
                                st506.lsect=0;
                                st506.lhead++;
                                if (st506.lhead==8)
                                {
                                        st506.lhead=0;
                                        st506.lcyl++;
                                        if (st506.lcyl>1023)
                                        {
                                                error("Hit limit\n");
                                                exit(-1);
                                        }
                                }
                        }
                        st506.oplen--;
                        for (c=16;c<272;c+=2)
                        {
                                temp=st506buffer[c];
                                st506buffer[c]=st506buffer[c+1];
                                st506buffer[c+1]=temp;
                        }
//                        rpclog("Write ST506buffer to %08X\n",ftell(hdfile));
//                        if (ftell(hdfile)==0x2048C00) dumpst506buffer();
                        fwrite(st506buffer+16,256,1,shdfile[st506.drive]);
//                        rpclog("ST506 OPLEN %i\n",st506.oplen);
                        if (st506.oplen)
                        {
                                st506.p=16;
                                st506.drq=1;
                                st506_updateinterrupts();
//                                rpclog("Write HDC interrupt part\n");
                        }
                        else
                        {
                                for (c=9;c>=0;c--)
                                    st506.param[c+2]=st506.param[c];
                                st506.param[0]=st506.param[1]=0;
                                st506.p=st506.wp=0;
//                                rpclog("Finished write sector! %02X\n",st506.OM1);
                                st506.status|=COMEND|PARAMREJECT;
                                st506.status&=~0x80;
                                st506.drq=0;
                                st506_updateinterrupts();
//                                rpclog("Write HDC interrupt full\n");
//                                output=1;
                        }
                }
                break;
                case 0xA3: /*Write format*/
                if (st506.first)
                {
                        st506.p=16;
                        st506.drq=1;
                        st506_updateinterrupts();
//                        rpclog("Write HDC interrupt first\n");
                        st506.first=0;
                }
                else
                {
                        c=0;
                        while (c<256 && st506.oplen)
                        {
                                st506.lsect++;
                                if (st506.lsect==32)
                                {
                                        st506.lsect=0;
                                        st506.lhead++;
                                        if (st506.lhead==8)
                                        {
                                                st506.lhead=0;
                                                st506.lcyl++;
                                                if (st506.lcyl>1023)
                                                {
                                                        error("Hit limit\n");
                                                        exit(-1);
                                                }
                                        }
                                }
                                st506.oplen--;
                                fwrite(st506buffer+16,256,1,shdfile[st506.drive]);
                                c+=4;
                        }
                        if (st506.oplen)
                        {
                                st506.p=16;
                                st506.drq=1;
                                st506_updateinterrupts();
                        }
                        else
                        {
                                for (c=9;c>=0;c--)
                                    st506.param[c+2]=st506.param[c];
                                st506.param[0]=st506.param[1]=0;
                                st506.p=st506.wp=0;
                                st506.status|=COMEND|PARAMREJECT;
                                st506.status&=~0x80;
                                st506.drq=0;
                                st506_updateinterrupts();
                        }
                }
                break;
        }
}
