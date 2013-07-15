/*Arculator 0.8 by Tom Walker
  I2C + CMOS RAM emulation*/
#include <stdio.h>
#include <allegro.h>
#ifdef WIN32
#include <winalleg.h>
#endif
#include "arc.h"

FILE *olog;
int output;
//uint32_t *armregs[16];
int cmosstate=0;
int i2cstate=0;
int lastdata;
uint8_t i2cbyte;
int i2cclock=1,i2cdata=1,i2cpos;
int i2ctransmit=-1;
#ifdef WIN32
SYSTEMTIME systemtime;
#endif

#define CMOS 1
#define ARM -1

#define I2C_IDLE             0
#define I2C_RECIEVE          1
#define I2C_TRANSMIT         2
#define I2C_ACKNOWLEDGE      3
#define I2C_TRANSACKNOWLEDGE 4

#define CMOS_IDLE            0
#define CMOS_RECIEVEADDR     1
#define CMOS_RECIEVEDATA     2
#define CMOS_SENDDATA        3

uint8_t cmosaddr;
uint8_t cmosram[256];
int cmosrw;
FILE *cmosf;

void cmosgettime();

int romset;
void loadcmos()
{
        char fn[512];
        int c;
//        rpclog("Read cmos %i\n",romset);
        if (romset>3) return;
        switch (romset)
        {
                case 0: append_filename(fn,exname,"cmos/arthur/cmos.bin",511); break;
                case 1: append_filename(fn,exname,"cmos/riscos2/cmos.bin",511); break;
                case 2: append_filename(fn,exname,"cmos/riscos3_old/cmos.bin",511); break;
                case 3: append_filename(fn,exname,"cmos/riscos3_new/cmos.bin",511); break;
        }
//        rpclog("Reading %s\n",fn);
        cmosf=fopen(fn,"rb");
        if (cmosf)
        {
                fread(cmosram,256,1,cmosf);
                fclose(cmosf);
        }
        else
           memset(cmosram,0,256);
        cmosgettime();
}

void savecmos()
{
        char fn[512];
//        rpclog("Writing CMOS %i\n",romset);
        if (romset>3) return;
        switch (romset)
        {
                case 0: append_filename(fn,exname,"cmos/arthur/cmos.bin",511); break;
                case 1: append_filename(fn,exname,"cmos/riscos2/cmos.bin",511); break;
                case 2: append_filename(fn,exname,"cmos/riscos3_old/cmos.bin",511); break;
                case 3: append_filename(fn,exname,"cmos/riscos3_new/cmos.bin",511); break;
        }
//        rpclog("Writing %s\n",fn);
        cmosf=fopen(fn,"wb");
//        for (c=0;c<256;c++)
//            putc(cmosram[(c-0x40)&0xFF],cmosf);
        fwrite(cmosram,256,1,cmosf);
        fclose(cmosf);
}

void cmosstop()
{
        cmosstate=CMOS_IDLE;
        i2ctransmit=ARM;
}

void cmosnextbyte()
{
//        cmosgettime();
        i2cbyte=cmosram[(cmosaddr++)&0xFF];
}
uint8_t cmosgetbyte()
{
        return cmosram[(cmosaddr++)&0xFF];
}
int rtcdisable=0;
void cmosgettime()
{
        int c,d;
        if (rtcdisable) return;
#ifdef WIN32
/*        GetLocalTime(&systemtime);
        c=systemtime.wMilliseconds/10;
        d=c%10;
        c/=10;
        cmosram[1]=d|(c<<4);
        d=systemtime.wSecond%10;
        c=systemtime.wSecond/10;
        cmosram[2]=d|(c<<4);
        d=systemtime.wMinute%10;
        c=systemtime.wMinute/10;
        cmosram[3]=d|(c<<4);
        d=systemtime.wHour%10;
        c=systemtime.wHour/10;
        cmosram[4]=d|(c<<4);
        d=systemtime.wDay%10;
        c=systemtime.wDay/10;
        cmosram[5]=d|(c<<4);
        d=systemtime.wMonth%10;
        c=systemtime.wMonth/10;
        cmosram[6]=d|(c<<4);*/
#endif
/*        d=systemtime.wYear%10;
        c=(systemtime.wYear/10)%10;
        cmosram[128]=d|(c<<4);
        systemtime.wYear/=100;
        d=systemtime.wYear%10;
        c=systemtime.wYear/10;
        cmosram[129]=d|(c<<4);*/
//        rpclog("Read time - %02X %02X %02X %02X %02X %02X\n",cmosram[1],cmosram[2],cmosram[3],cmosram[4],cmosram[5],cmosram[6]);
/*        if (!olog) olog=fopen("olog.txt","wt");
        sprintf(s,"Read time - %02X %02X %02X %02X %02X %02X\n",cmosram[1],cmosram[2],cmosram[3],cmosram[4],cmosram[5],cmosram[6]);
        fputs(s,olog);*/
}

void cmostick()
{
#ifdef WIN32
        systemtime.wMilliseconds+=2;
        if (systemtime.wMilliseconds>=100)
        {
                systemtime.wMilliseconds-=100;
                systemtime.wSecond++;
                if (systemtime.wSecond>=60)
                {
                        systemtime.wSecond-=60;
                        systemtime.wMinute++;
                        if (systemtime.wMinute>=60)
                        {
                                systemtime.wHour++;
                                if (systemtime.wMinute>=24)
                                {
                                        systemtime.wHour=0;
                                        systemtime.wDay++;
                                }
                        }
                }
        }
#endif
}

void cmoswrite(uint8_t byte)
{
        switch (cmosstate)
        {
                case CMOS_IDLE:
                cmosrw=byte&1;
                if (cmosrw)
                {
                        cmosstate=CMOS_SENDDATA;
                        i2ctransmit=CMOS;
                        if (cmosaddr<0x10) cmosgettime();
/*                        if (!olog) olog=fopen("olog.txt","wt");
                        sprintf(s,"Read CMOS %02X - %02X\n",cmosaddr,cmosram[cmosaddr]);
                        fputs(s,olog);*/
                        i2cbyte=cmosram[((cmosaddr++))&0xFF];
//printf("CMOS - %02X from %02X\n",i2cbyte,cmosaddr-1);
//                        log("Transmitter now CMOS\n");
                }
                else
                {
                        cmosstate=CMOS_RECIEVEADDR;
                        i2ctransmit=ARM;
                }
//                log("CMOS R/W=%i\n",cmosrw);
                return;

                case CMOS_RECIEVEADDR:
//                printf("CMOS addr=%02X\n",byte);
//                log("CMOS addr=%02X\n",byte);
                cmosaddr=byte;
                if (cmosrw)
                   cmosstate=CMOS_SENDDATA;
                else
                   cmosstate=CMOS_RECIEVEDATA;
                break;

                case CMOS_RECIEVEDATA:
//                printf("CMOS write %02X %02X\n",cmosaddr,byte);
//                log("%02X now %02X\n",cmosaddr,byte);
//                        sprintf(s,"Write CMOS %02X - %02X\n",cmosaddr,byte);
//                        fputs(s,olog);
                cmosram[((cmosaddr++))&0xFF]=byte;
                break;

                case CMOS_SENDDATA:
//                closevideo();
                printf("Send data %02X\n",cmosaddr);
                exit(-1);
        }
}

void cmosi2cchange(int nuclock, int nudata)
{
//        printf("I2C %i %i %i %i  %i\n",i2cclock,nuclock,i2cdata,nudata,i2cstate);
//        log("I2C update clock %i %i data %i %i state %i\n",i2cclock,nuclock,i2cdata,nudata,i2cstate);
        switch (i2cstate)
        {
                case I2C_IDLE:
                if (i2cclock && nuclock)
                {
                        if (lastdata && !nudata) /*Start bit*/
                        {
//                                printf("Start bit\n");
//                                log("Start bit recieved\n");
                                i2cstate=I2C_RECIEVE;
                                i2cpos=0;
                        }
                }
                break;

                case I2C_RECIEVE:
                if (!i2cclock && nuclock)
                {
//                        printf("Reciving %07X %07X\n",(*armregs[15]-8)&0x3FFFFFC,(*armregs[14]-8)&0x3FFFFFC);
                        i2cbyte<<=1;
                        if (nudata)
                           i2cbyte|=1;
                        else
                           i2cbyte&=0xFE;
                        i2cpos++;
                        if (i2cpos==8)
                        {
                        
//                                if (output) //logfile("Complete - byte %02X %07X %07X\n",i2cbyte,(*armregs[15]-8)&0x3FFFFFC,(*armregs[14]-8)&0x3FFFFFC);
                                cmoswrite(i2cbyte);
                                i2cstate=I2C_ACKNOWLEDGE;
                        }
                }
                else if (i2cclock && nuclock && nudata && !lastdata) /*Stop bit*/
                {
//                        log("Stop bit recieved\n");
                        i2cstate=I2C_IDLE;
                        cmosstop();
                }
                else if (i2cclock && nuclock && !nudata && lastdata) /*Start bit*/
                {
//                        log("Start bit recieved\n");
                        i2cpos=0;
                        cmosstate=CMOS_IDLE;
                }
                break;

                case I2C_ACKNOWLEDGE:
                if (!i2cclock && nuclock)
                {
//                        log("Acknowledging transfer\n");
                        nudata=0;
                        i2cpos=0;
                        if (i2ctransmit==ARM)
                           i2cstate=I2C_RECIEVE;
                        else
                           i2cstate=I2C_TRANSMIT;
                }
                break;

                case I2C_TRANSACKNOWLEDGE:
                if (!i2cclock && nuclock)
                {
                        if (nudata) /*It's not acknowledged - must be end of transfer*/
                        {
//                                printf("End of transfer\n");
                                i2cstate=I2C_IDLE;
                                cmosstop();
                        }
                        else /*Next byte to transfer*/
                        {
                                i2cstate=I2C_TRANSMIT;
                                cmosnextbyte();
                                i2cpos=0;
//                                printf("Next byte - %02X\n",i2cbyte);
                        }
                }
                break;

                case I2C_TRANSMIT:
                if (!i2cclock && nuclock)
                {
                        i2cdata=nudata=i2cbyte&128;
                        i2cbyte<<=1;
                        i2cpos++;
//                        if (output) //logfile("Transfering bit at %07X %i %02X\n",(*armregs[15]-8)&0x3FFFFFC,i2cpos,cmosaddr);
                        if (i2cpos==8)
                        {
                                i2cstate=I2C_TRANSACKNOWLEDGE;
//                                printf("Acknowledge mode\n");
                        }
                        i2cclock=nuclock;
                        return;
                }
                break;

        }
        if (!i2cclock && nuclock)
           i2cdata=nudata;
        lastdata=nudata;
        i2cclock=nuclock;
}
