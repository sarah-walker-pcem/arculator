/*Arculator 2.1 by Sarah Walker
  Disc support*/
#include <stdio.h>
#include <string.h>
#include "arc.h"
#include "config.h"

#include "disc.h"

#include "disc_adf.h"
#include "disc_apd.h"
#include "disc_fdi.h"
#include "disc_hfe.h"
#include "disc_jfd.h"

#include "ddnoise.h"

#include "ioc.h"
#include "timer.h"

char discname[4][512];
int defaultwriteprot = 0;
int disc_noise_gain;

static emu_timer_t disc_timer;

disc_funcs_t *drive_funcs[4];

int disc_drivesel = 0;
static int disc_notfound = 0;

int curdrive = 0;
int discchange[4];
int fdctype;
int readflash[4];
int motoron;
int writeprot[4];

static int disc_current_track[4] = {0, 0, 0, 0};

static uint64_t disc_poll_time;
static const int disc_poll_times[4] =
{
        32, /*Double density*/
        32, /*Double density*/
        16, /*High density*/
        8   /*Extended density - supported by SuperIO but never used on the Arc*/
};

fdc_funcs_t *fdc_funcs;
void *fdc_p;
emu_timer_t *fdc_timer;
int fdc_overridden;
int fdc_ready;

static struct
{
        char *ext;
        void (*load)(int drive, char *fn);
        int size;
}
loaders[]=
{
        {"SSD", ssd_load,         80*10* 256},
        {"DSD", dsd_load,       2*80*10* 256},
        {"ADF", adf_load,         80*16* 256},
        {"ADF", adf_arcdd_load, 2*80* 5*1024},
        {"ADF", adf_archd_load, 2*80*10*1024},
        {"ADL", adl_load,       2*80*16* 256},
        {"FDI", fdi_load,       -1},
        {"APD", apd_load,       -1},
        {"HFE", hfe_load,       -1},
//        {"JFD", jfd_load,       -1},
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
                        return;
                }
                c++;
        }
//        printf("Couldn't load %s %s\n",fn,p);
        /*No extension match, so guess based on image size*/
        rpclog("Size %i\n", size);
        if (size == (1680*1024)) /*1680k DOS - 80*2*21*512*/
        {
                driveloaders[drive] = 3;
                adf_loadex(drive, fn, 21, 512, 1, 0, 2, 1);
                return;
        }
        if (size == (1440*1024)) /*1440k DOS - 80*2*18*512*/
        {
                driveloaders[drive] = 3;
                adf_loadex(drive, fn, 18, 512, 1, 0, 2, 1);
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
                adf_loadex(drive, fn, 9, 512, 1, 0, 1, 1);
                return;
        }
        if (size == (360*1024)) /*360k DOS - 40*2*9*512*/
        {
                driveloaders[drive] = 3;
                adf_loadex(drive, fn, 9, 512, 1, 1, 1, 1);
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
        if (drive_funcs[drive])
        {
                drive_funcs[drive]->close(drive);
                drive_funcs[drive] = NULL;
        }
        disc_stop(drive);
}

int disc_empty(int drive)
{
        return !drive_funcs[drive];
}

void disc_init()
{
        memset(drive_funcs, 0, sizeof(drive_funcs));
}

void disc_reset()
{
        curdrive = 0;
        timer_add(&disc_timer, disc_poll, NULL, 0);
}

void disc_poll(void *p)
{
        timer_advance_u64(&disc_timer, disc_poll_time);
        if (drive_funcs[disc_drivesel])
        {
                if (drive_funcs[disc_drivesel]->poll)
                        drive_funcs[disc_drivesel]->poll();
                if (disc_notfound)
                {
                        disc_notfound--;
                        if (!disc_notfound)
                                fdc_funcs->notfound(fdc_p);
                }
        }
}

int disc_get_current_track(int drive)
{
        return disc_current_track[drive];
}

void disc_seek(int drive, int new_track)
{
        if (drive_funcs[drive] && drive_funcs[drive]->seek)
                drive_funcs[drive]->seek(drive, new_track);
        if (new_track != disc_current_track[drive] && !disc_empty(drive))
                ioc_discchange_clear(drive);
        ddnoise_seek(new_track - disc_current_track[drive]);
        disc_current_track[drive] = new_track;
}

void disc_readsector(int drive, int sector, int track, int side, int density)
{
        if (drive_funcs[drive] && drive_funcs[drive]->readsector)
                drive_funcs[drive]->readsector(drive, sector, track, side, density);
        else
                disc_notfound = 10000;
}

void disc_writesector(int drive, int sector, int track, int side, int density)
{
        if (drive_funcs[drive] && drive_funcs[drive]->writesector)
                drive_funcs[drive]->writesector(drive, sector, track, side, density);
        else
                disc_notfound = 10000;
}

void disc_readaddress(int drive, int track, int side, int density)
{
        if (drive_funcs[drive] && drive_funcs[drive]->readaddress)
                drive_funcs[drive]->readaddress(drive, track, side, density);
        else
                disc_notfound = 10000;
}

void disc_format(int drive, int track, int side, int density)
{
        if (drive_funcs[drive] && drive_funcs[drive]->format)
                drive_funcs[drive]->format(drive, track, side, density);
        else
                disc_notfound = 10000;
}

void disc_stop(int drive)
{
        if (drive_funcs[drive] && drive_funcs[drive]->stop)
                drive_funcs[drive]->stop();
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
