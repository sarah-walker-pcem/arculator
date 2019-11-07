/*Arculator 2.0 by Sarah Walker
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

int cmos_changed = 0;
int i2c_clock = 1, i2c_data = 1;

#define TRANSMITTER_CMOS 1
#define TRANSMITTER_ARM -1

#define I2C_IDLE             0
#define I2C_RECEIVE          1
#define I2C_TRANSMIT         2
#define I2C_ACKNOWLEDGE      3
#define I2C_TRANSACKNOWLEDGE 4

#define CMOS_IDLE            0
#define CMOS_RECEIVEADDR     1
#define CMOS_RECEIVEDATA     2
#define CMOS_SENDDATA        3

static struct
{
        int state;
        int last_data;
        int pos;
        int transmit;
        uint8_t byte;
} i2c;

static struct
{
        int state;
        int addr;
        int rw;
        
        uint8_t ram[256];

        emu_timer_t timer;
} cmos;

static struct
{
        int msec;
        int sec;
        int min;
        int hour;
        int day;
        int mon;
        int year;
} systemtime;

static const int rtc_days_in_month[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static void cmos_get_time();

void cmos_load()
{
        char fn[512];
        char cmos_name[512];
        FILE *cmosf;

        LOG_CMOS("Read cmos %i\n", romset);
        snprintf(cmos_name, sizeof(cmos_name), "cmos/%s.%s.cmos.bin", machine_config_name, config_get_cmos_name(romset, fdctype));
        append_filename(fn, exname, cmos_name, 511);

        cmosf = fopen(fn, "rb");
        if (cmosf)
        {
                fread(cmos.ram, 256, 1, cmosf);
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
                        fread(cmos.ram, 256, 1, cmosf);
                        fclose(cmosf);
                }
                else
                        memset(cmos.ram, 0, 256);
        }
        cmos_get_time();
}

void cmos_save()
{
        char fn[512];
        char cmos_name[512];
        FILE *cmosf;
        
        LOG_CMOS("Writing CMOS %i\n", romset);
        snprintf(cmos_name, sizeof(cmos_name), "cmos/%s.%s.cmos.bin", machine_config_name, config_get_cmos_name(romset, fdctype));
        append_filename(fn, exname, cmos_name, 511);

        LOG_CMOS("Writing %s\n", fn);
        cmosf = fopen(fn, "wb");
        fwrite(cmos.ram, 256, 1, cmosf);
        fclose(cmosf);
}

static void cmos_stop()
{
        LOG_CMOS("cmos_stop()\n");
        cmos.state = CMOS_IDLE;
        i2c.transmit = TRANSMITTER_ARM;
}

static void cmos_next_byte()
{
        LOG_CMOS("cmos_next_byte(%d)\n", cmos.addr);
        i2c.byte = cmos.ram[(cmos.addr++) & 0xFF];
}

static void cmos_get_time()
{
        int c, d;
        
        LOG_CMOS("cmos_get_time()\n");

        c = systemtime.msec / 10;
        d = c % 10;
        c /= 10;
        cmos.ram[1] = d | (c << 4);
        d = systemtime.sec % 10;
        c = systemtime.sec / 10;
        cmos.ram[2] = d | (c << 4);
        d = systemtime.min % 10;
        c = systemtime.min / 10;
        cmos.ram[3] = d | (c << 4);
        d = systemtime.hour % 10;
        c = systemtime.hour / 10;
        cmos.ram[4] = d | (c << 4);
        d = systemtime.day % 10;
        c = systemtime.day / 10;
        cmos.ram[5] = d | (c << 4);
        d = systemtime.mon % 10;
        c = systemtime.mon / 10;
        cmos.ram[6] = d | (c << 4);
//        LOG_CMOS("Read time - %02X %02X %02X %02X %02X %02X\n",cmosram[1],cmosram[2],cmosram[3],cmosram[4],cmosram[5],cmosram[6]);
}

static void cmos_tick(void *p)
{
        timer_advance_u64(&cmos.timer, TIMER_USEC * 10000); /*10ms*/
        
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

        timer_add(&cmos.timer, cmos_tick, NULL, 1);
}

void cmos_write(uint8_t byte)
{
        LOG_CMOS("cmos_write()\n");
        switch (cmos.state)
        {
                case CMOS_IDLE:
                cmos.rw = byte & 1;
                if (cmos.rw)
                {
                        cmos.state = CMOS_SENDDATA;
                        i2c.transmit = TRANSMITTER_CMOS;
                        if (cmos.addr < 0x10)
                                cmos_get_time();
                        i2c.byte = cmos.ram[(cmos.addr++) & 0xFF];
//printf("CMOS - %02X from %02X\n",i2cbyte,cmosaddr-1);
//                        log("Transmitter now CMOS\n");
                }
                else
                {
                        cmos.state = CMOS_RECEIVEADDR;
                        i2c.transmit = TRANSMITTER_ARM;
                }
//                log("CMOS R/W=%i\n",cmosrw);
                return;

                case CMOS_RECEIVEADDR:
//                printf("CMOS addr=%02X\n",byte);
//                log("CMOS addr=%02X\n",byte);
                cmos.addr = byte;
                if (cmos.rw)
                        cmos.state = CMOS_SENDDATA;
                else
                        cmos.state = CMOS_RECEIVEDATA;
                break;

                case CMOS_RECEIVEDATA:
//                printf("CMOS write %02X %02X\n",cmosaddr,byte);
//                log("%02X now %02X\n",cmosaddr,byte);
                cmos.ram[(cmos.addr++) & 0xFF] = byte;
                if (!cmos_changed)
                        cmos_changed = CMOS_CHANGE_DELAY;
                break;

                case CMOS_SENDDATA:
#ifndef RELEASE_BUILD
                fatal("Send data %02X\n", cmos.addr);
#endif
                break;
        }
}

void i2c_change(int new_clock, int new_data)
{
//        printf("I2C %i %i %i %i  %i\n",i2cclock,nuclock,i2cdata,nudata,i2cstate);
//        log("I2C update clock %i %i data %i %i state %i\n",i2cclock,nuclock,i2cdata,nudata,i2cstate);
        switch (i2c.state)
        {
                case I2C_IDLE:
                if (i2c_clock && new_clock)
                {
                        if (i2c.last_data && !new_data) /*Start bit*/
                        {
//                                printf("Start bit\n");
//                                log("Start bit received\n");
                                i2c.state = I2C_RECEIVE;
                                i2c.pos = 0;
                        }
                }
                break;

                case I2C_RECEIVE:
                if (!i2c_clock && new_clock)
                {
//                        printf("Reciving %07X %07X\n",(*armregs[15]-8)&0x3FFFFFC,(*armregs[14]-8)&0x3FFFFFC);
                        i2c.byte <<= 1;
                        if (new_data)
                                i2c.byte |= 1;
                        else
                                i2c.byte &= 0xFE;
                        i2c.pos++;
                        if (i2c.pos == 8)
                        {
                        
//                                if (output) //logfile("Complete - byte %02X %07X %07X\n",i2cbyte,(*armregs[15]-8)&0x3FFFFFC,(*armregs[14]-8)&0x3FFFFFC);
                                cmos_write(i2c.byte);
                                i2c.state = I2C_ACKNOWLEDGE;
                        }
                }
                else if (i2c_clock && new_clock && new_data && !i2c.last_data) /*Stop bit*/
                {
//                        log("Stop bit received\n");
                        i2c.state = I2C_IDLE;
                        cmos_stop();
                }
                else if (i2c_clock && new_clock && !new_data && i2c.last_data) /*Start bit*/
                {
//                        log("Start bit received\n");
                        i2c.pos = 0;
                        cmos.state = CMOS_IDLE;
                }
                break;

                case I2C_ACKNOWLEDGE:
                if (!i2c_clock && new_clock)
                {
//                        log("Acknowledging transfer\n");
                        new_data = 0;
                        i2c.pos = 0;
                        if (i2c.transmit == TRANSMITTER_ARM)
                                i2c.state = I2C_RECEIVE;
                        else
                                i2c.state = I2C_TRANSMIT;
                }
                break;

                case I2C_TRANSACKNOWLEDGE:
                if (!i2c_clock && new_clock)
                {
                        if (new_data) /*It's not acknowledged - must be end of transfer*/
                        {
//                                printf("End of transfer\n");
                                i2c.state = I2C_IDLE;
                                cmos_stop();
                        }
                        else /*Next byte to transfer*/
                        {
                                i2c.state = I2C_TRANSMIT;
                                cmos_next_byte();
                                i2c.pos = 0;
//                                printf("Next byte - %02X\n",i2cbyte);
                        }
                }
                break;

                case I2C_TRANSMIT:
                if (!i2c_clock && new_clock)
                {
                        i2c_data = new_data = i2c.byte & 128;
                        i2c.byte <<= 1;
                        i2c.pos++;
//                        if (output) //logfile("Transfering bit at %07X %i %02X\n",(*armregs[15]-8)&0x3FFFFFC,i2cpos,cmosaddr);
                        if (i2c.pos == 8)
                        {
                                i2c.state = I2C_TRANSACKNOWLEDGE;
//                                printf("Acknowledge mode\n");
                        }
                        i2c_clock = new_clock;
                        return;
                }
                break;

        }
        if (!i2c_clock && new_clock)
                i2c_data = new_data;
        i2c.last_data = new_data;
        i2c_clock = new_clock;
}
