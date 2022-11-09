#include "timer.h"

typedef struct disc_funcs_t
{
	void (*seek)(int drive, int track);
	void (*readsector)(int drive, int sector, int track, int side, int density);
	void (*writesector)(int drive, int sector, int track, int side, int density);
	void (*readaddress)(int drive, int track, int side, int density);
	void (*format)(int drive, int track, int side, int density);
	void (*stop)();
	void (*poll)();
	void (*close)(int drive);
	int high_res_poll;
} disc_funcs_t;

extern disc_funcs_t *drive_funcs[4];

typedef struct fdc_funcs_t
{
	void (*data)(uint8_t dat, void *p);
	void (*spindown)(void *p);
	void (*finishread)(void *p);
	void (*notfound)(void *p);
	void (*datacrcerror)(void *p);
	void (*headercrcerror)(void *p);
	void (*writeprotect)(void *p);
	int  (*getdata)(int last, void *p);
	void (*sectorid)(uint8_t track, uint8_t side, uint8_t sector, uint8_t size, uint8_t crc1, uint8_t crc2, void *p);
	void (*indexpulse)(void *p);
} fdc_funcs_t;

extern fdc_funcs_t *fdc_funcs;
extern emu_timer_t *fdc_timer;
extern void *fdc_p;

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

extern int fdc_ready;
extern int fdc_overridden;

extern int motoron;

extern int defaultwriteprot;

extern int writeprot[4];

extern int disc_noise_gain;
#define DISC_NOISE_DISABLED 9999
