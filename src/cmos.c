/*Arculator 0.8 by Tom Walker
  I2C + CMOS RAM emulation*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WIN32
#include <windows.h>
#endif
#include "arc.h"
#include "cmos.h"
#include "config.h"
#include "timer.h"

static timer_t cmos_timer;

int cmos_changed = 0;

int cmosstate=0;
int i2cstate=0;
int lastdata;
uint8_t i2cbyte;
int i2cclock=1,i2cdata=1,i2cpos;
int i2ctransmit=-1;

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

typedef struct tod_t
{
        int msec;
        int sec;
        int min;
        int hour;
        int day;
        int mon;
        int year;
} tod_t;

static tod_t systemtime;

static int rtc_days_in_month[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

void cmosgettime();

void loadcmos()
{
        char fn[512];
        char cmos_name[512];
        
        LOG_CMOS("Read cmos %i\n",romset);
        snprintf(cmos_name, sizeof(cmos_name), "cmos/%s.%s.cmos.bin", machine_config_name, config_get_cmos_name(romset, fdctype));
        append_filename(fn, exname, cmos_name, 511);

        cmosf=fopen(fn,"rb");
        if (cmosf)
        {
                fread(cmosram,256,1,cmosf);
                fclose(cmosf);
                LOG_CMOS("Read CMOS contents from %s\n", fn);
        }
        else
        {
                LOG_CMOS("%s doesn't exist; resetting CMOS\n", fn);
                snprintf(cmos_name, sizeof(cmos_name), "cmos/%s/cmos.bin", config_get_cmos_name(romset, fdctype));
                append_filename(fn, exname, cmos_name, 511);

                cmosf = fopen(fn, "rb");
                if (cmosf)
                {
                        fread(cmosram, 256, 1, cmosf);
                        fclose(cmosf);
                }
                else
                        memset(cmosram,0,256);
        }
        cmosgettime();
}

void savecmos()
{
        char fn[512];
        char cmos_name[512];
        
        LOG_CMOS("Writing CMOS %i\n",romset);
        snprintf(cmos_name, sizeof(cmos_name), "cmos/%s.%s.cmos.bin", machine_config_name, config_get_cmos_name(romset, fdctype));
        append_filename(fn, exname, cmos_name, 511);

        LOG_CMOS("Writing %s\n",fn);
        cmosf=fopen(fn,"wb");
        fwrite(cmosram,256,1,cmosf);
        fclose(cmosf);
}

void cmosstop()
{
        LOG_CMOS("cmosstop()\n");
        cmosstate=CMOS_IDLE;
        i2ctransmit=ARM;
}

void cmosnextbyte()
{
        LOG_CMOS("cmosnextbyte(%d)\n", cmosaddr);
        i2cbyte=cmosram[(cmosaddr++)&0xFF];
}

void cmosgettime()
{
        int c, d;
        
        LOG_CMOS("cmosgettime()\n");

        c = systemtime.msec / 10;
        d = c % 10;
        c /= 10;
        cmosram[1] = d | (c << 4);
        d = systemtime.sec % 10;
        c = systemtime.sec / 10;
        cmosram[2] = d | (c << 4);
        d = systemtime.min % 10;
        c = systemtime.min / 10;
        cmosram[3] = d | (c << 4);
        d = systemtime.hour % 10;
        c = systemtime.hour / 10;
        cmosram[4] = d | (c << 4);
        d = systemtime.day % 10;
        c = systemtime.day / 10;
        cmosram[5] = d | (c << 4);
        d = systemtime.mon % 10;
        c = systemtime.mon / 10;
        cmosram[6] = d | (c << 4);
//        LOG_CMOS("Read time - %02X %02X %02X %02X %02X %02X\n",cmosram[1],cmosram[2],cmosram[3],cmosram[4],cmosram[5],cmosram[6]);
}

void cmos_tick(void *p)
{
        timer_advance_u64(&cmos_timer, TIMER_USEC * 10000); /*10ms*/
        
        systemtime.msec += 10;
        if (systemtime.msec >= 1000)
        {
                systemtime.msec = 0;
                systemtime.sec++;
                if (systemtime.sec >= 60)
                {
                        systemtime.sec = 0;
                        systemtime.min++;
                        if (systemtime.min >= 60)
                        {
                                systemtime.min = 0;
                                systemtime.hour++;
                                if (systemtime.hour >= 24)
                                {
                                        systemtime.hour = 0;
                                        systemtime.day++;
                                        if (systemtime.day >= rtc_days_in_month[systemtime.mon])
                                        {
                                                systemtime.day = 0;
                                                systemtime.mon++;
                                                if (systemtime.mon >= 12)
                                                {
                                                        systemtime.mon = 0;
                                                        systemtime.year++;
                                                }
                                        }
                                }
                        }
                }
        }
}

void cmos_init()
{
#ifdef WIN32
        SYSTEMTIME real_time;

        GetLocalTime(&real_time);
        systemtime.msec = real_time.wMilliseconds;
        systemtime.sec = real_time.wSecond;
        systemtime.min = real_time.wMinute;
        systemtime.hour = real_time.wHour;
        systemtime.day = real_time.wDay;
        systemtime.mon = real_time.wMonth;
        systemtime.year = real_time.wYear;
#endif

        timer_add(&cmos_timer, cmos_tick, NULL, 1);
}

void cmoswrite(uint8_t byte)
{
        LOG_CMOS("cmoswrite()\n");
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
                if (!cmos_changed)
                        cmos_changed = CMOS_CHANGE_DELAY;
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
