/*Arculator 2.0 by Sarah Walker
  Disc support*/
#include <stdio.h>
#include <string.h>
#include "arc.h"
#include "config.h"

#include "disc.h"

#include "disc_adf.h"
#include "disc_apd.h"
#include "disc_fdi.h"
#include "disc_jfd.h"
#include "disc_ssd.h"

#include "ddnoise.h"

#include "ioc.h"
#include "timer.h"

static emu_timer_t disc_timer;

int disc_drivesel = 0;

int disc_noise_gain;

DRIVE drives[4];

char discname[4][512];
int curdrive = 0;
int discchange[4];
uint8_t disc[4][2][80][16][1024]; /*Disc - E format (2 sides, 80 tracks, 5 sectors, 1024 bytes)*/
int fdctype;
int readflash[4];
int fastdisc;

char discfns[4][260] = {"", ""};
int defaultwriteprot = 0;


static uint64_t disc_poll_time;
static const int disc_poll_times[4] =
{
        32, /*Double density*/
        32, /*Double density*/
        16, /*High density*/
        8   /*Extended density - supported by SuperIO but never used on the Arc*/
};

int fdc_ready;

static int drive_empty[4] = {1, 1, 1, 1};

int motorspin;
int motoron;

int fdc_indexcount = 52;

void (*fdc_data)(uint8_t dat);
void (*fdc_spindown)();
void (*fdc_finishread)();
void (*fdc_notfound)();
void (*fdc_datacrcerror)();
void (*fdc_headercrcerror)();
void (*fdc_writeprotect)();
int  (*fdc_getdata)(int last);
void (*fdc_sectorid)(uint8_t track, uint8_t side, uint8_t sector, uint8_t size, uint8_t crc1, uint8_t crc2);
void (*fdc_indexpulse)();

emu_timer_t fdc_timer;

static struct
{
        char *ext;
        void (*load)(int drive, char *fn);
        void (*close)(int drive);
        int size;
}
loaders[]=
{
        {"SSD", ssd_load,       ssd_close,   80*10* 256},
        {"DSD", dsd_load,       ssd_close, 2*80*10* 256},
        {"ADF", adf_load,       adf_close,   80*16* 256},
        {"ADF", adf_arcdd_load, adf_close, 2*80* 5*1024},
        {"ADF", adf_archd_load, adf_close, 2*80*10*1024},
        {"ADL", adl_load,       adf_close, 2*80*16* 256},
        {"FDI", fdi_load,       fdi_close, -1},
        {"APD", apd_load,       apd_close, -1},
//        {"JFD", jfd_load,       jfd_close, -1},
        {0,0,0}
};

static int driveloaders[4];

void disc_load(int drive, char *fn)
{
        int c = 0, size;
        char *p;
        FILE *f;
        rpclog("disc_load %i %s\n", drive, fn);
//        setejecttext(drive, "");
        if (!fn) return;
        p = get_extension(fn);
        if (!p) return;
//        setejecttext(drive, fn);
        rpclog("Loading :%i %s %s\n", drive, fn,p);
        f = fopen(fn, "rb");
        if (!f) return;
        fseek(f, -1, SEEK_END);
        size = ftell(f) + 1;
        fclose(f);        
        while (loaders[c].ext)
        {
                if (!strcasecmp(p, loaders[c].ext) && (size <= loaders[c].size || loaders[c].size == -1))
                {
                        rpclog("Loading as %s\n", p);
                        driveloaders[drive] = c;
                        loaders[c].load(drive, fn);
                        drive_empty[drive] = 0;
                        return;
                }
                c++;
        }
//        printf("Couldn't load %s %s\n",fn,p);
        /*No extension match, so guess based on image size*/
        rpclog("Size %i\n", size);
        drive_empty[drive] = 0;
        if (size == (1440*1024)) /*1440k DOS - 80*2*18*512*/
        {
                driveloaders[drive] = 3;
                adf_loadex(drive, fn, 18, 512, 1, 0, 2);
                return;
        }
        if (size == (800*1024)) /*800k ADFS/DOS - 80*2*5*1024*/
        {
                driveloaders[drive] = 2;
                loaders[2].load(drive, fn);
                return;
        }
        if (size == (640*1024)) /*640k ADFS/DOS - 80*2*16*256*/
        {
                driveloaders[drive] = 3;
                loaders[3].load(drive, fn);
                return;
        }
        if (size == (720*1024)) /*720k DOS - 80*2*9*512*/
        {
                driveloaders[drive] = 3;
                adf_loadex(drive, fn, 9, 512, 1, 0, 1);
                return;
        }
        if (size == (360*1024)) /*360k DOS - 40*2*9*512*/
        {
                driveloaders[drive] = 3;
                adf_loadex(drive, fn, 9, 512, 1, 1, 1);
                return;
        }
        if (size <= (200 * 1024)) /*200k DFS - 80*1*10*256*/
        {
                driveloaders[drive] = 0;
                loaders[0].load(drive, fn);
                return;
        }
        if (size <= (400 * 1024)) /*400k DFS - 80*2*10*256*/
        {
                driveloaders[drive] = 1;
                loaders[1].load(drive, fn);
                return;
        }
        drive_empty[drive] = 1;
}

void disc_new(int drive, char *fn)
{
        int c = 0, d;
        FILE *f;
        char *p = get_extension(fn);
        while (loaders[c].ext)
        {
                if (!strcasecmp(p, loaders[c].ext) && loaders[c].size != -1)
                {
                        f=fopen(fn, "wb");
                        for (d = 0; d < loaders[c].size; d++) putc(0, f);
                        if (!strcasecmp(p, "ADF"))
                        {
                                fseek(f, 0, SEEK_SET);
                                putc(7, f);
                                fseek(f, 0xFD, SEEK_SET);
                                putc(5, f); putc(0, f); putc(0xC, f); putc(0xF9, f); putc(0x04, f);
                                fseek(f, 0x1FB, SEEK_SET);
                                putc(0x88,f); putc(0x39,f); putc(0,f); putc(3,f); putc(0xC1,f);
                                putc(0, f); putc('H', f); putc('u', f); putc('g', f); putc('o', f);
                                fseek(f, 0x6CC, SEEK_SET);
                                putc(0x24, f);
                                fseek(f, 0x6D6, SEEK_SET);
                                putc(2, f); putc(0, f); putc(0, f); putc(0x24, f);
                                fseek(f, 0x6FB, SEEK_SET);
                                putc('H', f); putc('u', f); putc('g', f); putc('o', f);
                        }
                        if (!strcasecmp(p, "ADL"))
                        {
                                fseek(f, 0, SEEK_SET);
                                putc(7, f);
                                fseek(f, 0xFD, SEEK_SET);
                                putc(0xA, f); putc(0, f); putc(0x11, f); putc(0xF9, f); putc(0x09, f);
                                fseek(f, 0x1FB, SEEK_SET);
                                putc(0x01, f); putc(0x84, f); putc(0, f); putc(3, f); putc(0x8A, f);
                                putc(0, f); putc('H', f); putc('u', f); putc('g', f); putc('o', f);
                                fseek(f, 0x6CC, SEEK_SET);
                                putc(0x24, f);
                                fseek(f, 0x6D6, SEEK_SET);
                                putc(2, f); putc(0, f); putc(0, f); putc(0x24, f);
                                fseek(f, 0x6FB, SEEK_SET);
                                putc('H', f); putc('u', f); putc('g', f); putc('o', f);
                        }
                        fclose(f);
                        disc_load(drive, fn);
                        return;
                }
                c++;
        }
}

void disc_close(int drive)
{
        rpclog("disc_close %i\n", drive);
        if (!drive_empty[drive])
        {
                if (loaders[driveloaders[drive]].close)
                        loaders[driveloaders[drive]].close(drive);
                drive_empty[drive] = 1;
        }
        disc_stop(drive);
}

int disc_empty(int drive)
{
        return drive_empty[drive];
}

int disc_notfound=0;

void disc_init()
{
        drives[0].poll = drives[1].poll = 0;
        drives[0].seek = drives[1].seek = 0;
        drives[0].readsector = drives[1].readsector = 0;
}

void disc_reset()
{
        curdrive = 0;
        timer_add(&disc_timer, disc_poll, NULL, 0);
}

void disc_poll(void *p)
{
        timer_advance_u64(&disc_timer, disc_poll_time);
        if (!drive_empty[disc_drivesel])
        {
                if (drives[disc_drivesel].poll) drives[disc_drivesel].poll();
                if (disc_notfound)
                {
                        disc_notfound--;
                        if (!disc_notfound)
                           fdc_notfound();
                }
        }
}

int oldtrack[4] = {0, 0, 0, 0};
void disc_seek(int drive, int track)
{
        if (drives[drive].seek)
                drives[drive].seek(drive, track);
        if (track != oldtrack[drive] && !disc_empty(drive))
                ioc_discchange_clear(drive);
        ddnoise_seek(track - oldtrack[drive]);
        oldtrack[drive] = track;
}

void disc_readsector(int drive, int sector, int track, int side, int density)
{
        if (drives[drive].readsector)
           drives[drive].readsector(drive, sector, track, side, density);
        else
           disc_notfound = 10000;
}

void disc_writesector(int drive, int sector, int track, int side, int density)
{
        if (drives[drive].writesector)
           drives[drive].writesector(drive, sector, track, side, density);
        else
           disc_notfound = 10000;
}

void disc_readaddress(int drive, int track, int side, int density)
{
        if (drives[drive].readaddress)
           drives[drive].readaddress(drive, track, side, density);
        else
           disc_notfound = 10000;
}

void disc_format(int drive, int track, int side, int density)
{
        if (drives[drive].format)
           drives[drive].format(drive, track, side, density);
        else
           disc_notfound = 10000;
}

void disc_stop(int drive)
{
        if (drives[drive].stop)
           drives[drive].stop();
}

void disc_set_motor(int enable)
{
        if (!enable)
                timer_disable(&disc_timer);
        else if (!timer_is_enabled(&disc_timer))
                timer_set_delay_u64(&disc_timer, disc_poll_time);
}

void disc_set_density(int density)
{
        disc_poll_time = disc_poll_times[density] * TIMER_USEC;
}
