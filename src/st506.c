#include <stdio.h>
#include "arc.h"

void callbackst506();
int printnext=0;
int timetolive;
#define BUSY            0x80
#define PARAMREJECT     0x40
#define COMEND          0x20
#define SEEKEND         0x10
#define ABNEND          0x04

FILE *hdfile;
int idecallback;

struct
{
        unsigned char status;
        unsigned char param[16];
        unsigned char cul;
        unsigned char command;
        int p,wp;
        int track;
        int lsect,lcyl,lhead;
        int oplen;
        unsigned char OM1;
        unsigned char ssb;
} st506;
unsigned char st506buffer[256];

void resetst506()
{
        return;
        st506.status=0;
        st506.p=st506.wp=0;
        fclose(hdfile);
        hdfile=fopen("HardImage1","rb");
}

#define TOV 0x58
#define IPH 0x3C
#define NSC 0x24

void st506error(unsigned char err)
{
        rpclog("ST506 error - %02X\n",err);
        st506.status=ABNEND|COMEND;
        if (!(st506.OM1&0x20))
        {
                ioc.irqb|=8;
                rpclog("HDC interrupt\n");
                updateirqs();
        }
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
void writest506(unsigned long a, unsigned char v)
{
        return;
        rpclog("Write HDC %08X %08X\n",a,v);
        error("ST506 write %08X %02X %02X at %07X\n",a,a&0x3C,v,PC);
        exit(-1);
}

unsigned char readst506(unsigned long a)
{
        unsigned short temp;
        return 0xFF;
        if (printnext)
        {
                printnext=0;
                rpclog("Read %i\n",idecallback);
        }
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
                        rpclog("Read HDC %08X %08X\n",a,temp,st506writes);
                        return temp;//st506.param[st506.p-1];
                }
                else if (st506.p<272)
                {
                        temp=st506buffer[st506.p++]<<8;
                        temp|=st506buffer[st506.p++];
                        if (st506.p==272)
                        {
                                ioc.irqb&=~8;
                                updateirqs();
                                idecallback=40;
                        }
                        rpclog("Read HDC %08X %08X %07X\n",a,temp,PC);
                        return temp;
                }
                else
                {
                        ioc.irqb&=~8;
                        updateirqs();
                        rpclog("Read HDC %08X %08X\n",a,0x23,st506writes);
                        return 0x23;
                }
                break;
        }
        error("ST506 read %08X %02X at %07X\n",a,a&0x3C,PC);
        exit(-1);
}

void writest506l(unsigned long a, unsigned long v)
{
        unsigned char temp;
        output=0;
        return;
        v>>=16;
        rpclog("Write HDC %08X %08X %08X\n",a&0x3C,v,st506writes);
        st506writes++;
        switch (a&0x3C)
        {
                case 0: /*New command*/
//                rpclog("New ST506 command %04X\n",v);
                if (st506.status&BUSY)
                {
                        rpclog("Command rejected\n");
                        return;
                }
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
                        return;
                        case 0x10: /*Enable polling*/
                        st506.status&=~PARAMREJECT;
                        return;
                        case 0x18: /*Disable polling*/
                        st506.status&=~PARAMREJECT;
                        return;
                        case 0x28: /*Check drive*/
                        temp=st506.param[1]&3;
                        st506.status=0;
                        st506.param[0]=0;
                        st506.param[1]=0;
                        st506.param[2]=temp;
                        st506.param[3]=0;
                        st506.param[4]=(st506.track)?0xC0:0xE0;
                        st506.param[5]=0;
                        return;
                        case 0x40: /*Read data*/
                        st506.lcyl=(st506.param[2]<<8)|st506.param[3];
                        st506.lhead=st506.param[4];
                        st506.lsect=st506.param[5];
                        st506.oplen=(st506.param[6]<<8)|st506.param[7];
                        rpclog("Read data : cylinder %i head %i sector %i   length %i sectors\n",st506.lcyl,st506.lhead,st506.lsect,st506.oplen);
                        if (st506.lcyl>1023) { st506error(NSC); readdataerror(); return; }
                        if (st506.lhead>7)   { st506error(IPH); readdataerror(); return; }
                        if (st506.lsect>31)  { st506error(TOV); readdataerror(); return; }
                        fseek(hdfile,(((((st506.lcyl*8)+st506.lhead)*32)+st506.lsect)*256),SEEK_SET);
                        rpclog("Seeked to %08X\n",(((((st506.lcyl*8)+st506.lhead)*32)+st506.lsect)*256));
                        idecallback=100;
                        st506.status|=0x80;
                        output=1;
//                        exit(-1);
                        return;
                        case 0xC0: /*Seek*/
                        st506.track=st506.param[3]|(st506.param[2]<<8);
//                        rpclog("Seek to track %i\n",st506.track);
                        st506.param[0]=0;
                        st506.param[1]=0;
                        st506.param[2]=0;
                        st506.param[3]=st506.cul;
                        st506.status|=COMEND;
                        st506.status|=SEEKEND;
                        if (!(st506.OM1&0x20))
                        {
                                rpclog("HDC interrupt\n");
                                ioc.irqb|=8;
                                updateirqs();
                        }
                        return;
                        case 0xC8: /*Recalibrate*/
                        st506.track=0;
//                        rpclog("Recalibrate : seek to track %i\n",st506.track);
                        st506.status|=SEEKEND;
                        st506.param[0]=0;
                        st506.param[1]=0;
                        st506.param[2]=0;
                        st506.param[3]=st506.cul;
                        return;
                        case 0xE8: /*Specify*/
                        rpclog("Specify\nOM1 = %02X\nSHRL = %02X\nSectors = %i\nHeads = %i\nCylinders = %i\n",st506.param[1],st506.param[8],st506.param[7]+1,st506.param[6]+1,(st506.param[5]|((st506.param[4]&3)<<8))+1);
                        st506.status=PARAMREJECT;
                        st506.OM1=st506.param[1];
                        st506.cul=st506.param[3];
                        st506.param[0]=0;
                        st506.param[1]=0;
                        rpclog("OM1=%02X\n",st506.OM1);
                        return;
                        case 0xF0: /*Abort*/
                        st506.status=(st506.status&PARAMREJECT)|COMEND;
                        st506.param[0]=0;
                        st506.param[1]=4;
                        if (!(st506.OM1&0x20))
                        {
                                rpclog("HDC interrupt\n");
                                ioc.irqb|=8;
                                updateirqs();
                        }
                        break;
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
        }
        error("ST506 writel %08X %02X %08X at %07X\n",a,a&0x3C,v,PC);
        exit(-1);
}

unsigned long readst506l(unsigned long a)
{
        unsigned short temp;
/*        if (printnext)
        {
                printnext=0;
                rpclog("Read %i\n",idecallback);
        }*/
//        st506writes++;
        return 0xFFFF;
        switch (a&0x3C)
        {
                case 0x08: /*DMA read*/
//                rpclog("Read DMA %i\n",st506.p);
                if (st506.p>=16 && st506.p<272)
                {
                        temp=st506buffer[st506.p++]<<8;
                        temp|=st506buffer[st506.p++];
                        if (st506.p==272)
                        {
                                if (temp==0x8100) output=1;
                                ioc.irqb&=~8;
                                updateirqs();
//                                callbackst506();
                                idecallback=20;
//                                rpclog("End of command %i\n",idecallback);
//                                printnext=1;
                        }
                        rpclog("Read HDC %08X %08X %i\n",a,temp,st506.p);
                        return temp;
                }
                else if (st506.p>16)
                {
                        ioc.irqb&=~8;
                        updateirqs();
                        rpclog("Read HDC %08X %08X\n",a,1223,st506writes);
                        return 0x1223;
                }
                return 0;
                case 0x20:
//                rpclog("Return ST506 status %04X %07X %02X %02X\n",st506.status<<8,PC,ioc.irqb,ioc.mskb);
//                output=1;
//                timetolive=6;
                rpclog("Read HDC %08X %08X\n",a,st506.status<<8,st506writes);
                return st506.status<<8;
        }
        error("ST506 readl %08X %02X at %07X\n",a,a&0x3C,PC);
        exit(-1);
}

void callbackst506()
{
        int c;
        unsigned char temp;
        return;
        rpclog("Callback!\n");
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
                        fread(st506buffer+16,256,1,hdfile);
                        for (c=16;c<272;c+=2)
                        {
                                temp=st506buffer[c];
                                st506buffer[c]=st506buffer[c+1];
                                st506buffer[c+1]=temp;
                        }
                        st506.p=16;
                        ioc.irqb|=8;
                        rpclog("HDC interrupt part\n");
                        updateirqs();
                }
                else
                {
                        for (c=9;c>=0;c--)
                            st506.param[c+2]=st506.param[c];
                        st506.param[0]=st506.param[1]=0;
                        st506.p=st506.wp=0;
                        rpclog("Finished read sector! %02X\n",st506.OM1);
                        st506.status|=COMEND|PARAMREJECT;
                        st506.status&=~0x80;
                        if (!(st506.OM1&0x20))
                        {
                                rpclog("HDC interrupt full\n");
                                ioc.irqb|=8;
                                updateirqs();
                        }
                }
                break;
        }
}
