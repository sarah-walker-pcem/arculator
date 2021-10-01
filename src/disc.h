#include "timer.h"

typedef struct
{
        void (*seek)(int drive, int track);
        void (*readsector)(int drive, int sector, int track, int side, int density);
        void (*writesector)(int drive, int sector, int track, int side, int density);
        void (*readaddress)(int drive, int track, int side, int density);
        void (*format)(int drive, int track, int side, int density);
        void (*stop)();
        void (*poll)();
} DRIVE;

extern DRIVE drives[4];

extern int curdrive;

void disc_load(int drive, char *fn);
void disc_new(int drive, char *fn);
void disc_close(int drive);
void disc_init();
void disc_reset();
void disc_poll();
void disc_seek(int drive, int track);
void disc_readsector(int drive, int sector, int track, int side, int density);
void disc_writesector(int drive, int sector, int track, int side, int density);
void disc_readaddress(int drive, int track, int side, int density);
void disc_format(int drive, int track, int side, int density);
void disc_stop(int drive);
int disc_empty(int drive);
void disc_set_motor(int enable);
void disc_set_density(int density);
int disc_get_current_track(int drive);
extern int disc_drivesel;

extern void (*fdc_data)(uint8_t dat, void *p);
extern void (*fdc_spindown)(void *p);
extern void (*fdc_finishread)(void *p);
extern void (*fdc_notfound)(void *p);
extern void (*fdc_datacrcerror)(void *p);
extern void (*fdc_headercrcerror)(void *p);
extern void (*fdc_writeprotect)(void *p);
extern int  (*fdc_getdata)(int last, void *p);
extern void (*fdc_sectorid)(uint8_t track, uint8_t side, uint8_t sector, uint8_t size, uint8_t crc1, uint8_t crc2, void *p);
extern void (*fdc_indexpulse)(void *p);
extern void *fdc_p;
extern int fdc_ready;
extern int fdc_indexcount;
extern int fdc_overridden;

extern int motorspin;
extern int motoron;

extern int defaultwriteprot;
extern char discfns[4][260];

extern int writeprot[4], fwriteprot[4];

extern emu_timer_t *fdc_timer;

extern int disc_noise_gain;
#define DISC_NOISE_DISABLED 9999
