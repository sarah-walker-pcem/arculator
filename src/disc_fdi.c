/*Arculator 2.1 by Sarah Walker
  FDI disc image support
  Interfaces with fdi2raw.c*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "arc.h"
#include "disc.h"
#include "disc_fdi.h"
#include "disc_mfm_common.h"
#include "fdi2raw.h"

static disc_funcs_t fdi_disc_funcs;

static void fdi_seek(int drive, int track);

static struct
{
	mfm_t mfm;
	FILE *f;
	FDI *h;

	int sides;
	int lasttrack;
} fdi[4];

static uint8_t fdi_timing[65536];

static int fdi_drive;

void fdi_init()
{
//        printf("FDI reset\n");
	fdi[0].f = fdi[1].f = fdi[2].f = fdi[3].f = 0;
}

void fdi_load(int drive, char *fn)
{
	writeprot[drive] = 1;
	fdi[drive].f = fopen(fn, "rb");
	if (!fdi[drive].f)
		return;
	fdi[drive].h = fdi2raw_header(fdi[drive].f);
//        if (!fdih[drive]) printf("Failed to load!\n");
	fdi[drive].lasttrack = fdi2raw_get_last_track(fdi[drive].h);
	fdi[drive].sides = (fdi[drive].lasttrack > 83) ? 1 : 0;
	fdi[drive].mfm.write_protected = 1;
//        printf("Last track %i\n",fdilasttrack[drive]);
	drive_funcs[drive] = &fdi_disc_funcs;
	rpclog("Loaded as FDI\n");
	fdi_seek(drive, disc_get_current_track(drive));
}

static void fdi_close(int drive)
{
	if (fdi[drive].h)
		fdi2raw_header_free(fdi[drive].h);
	if (fdi[drive].f)
		fclose(fdi[drive].f);
	fdi[drive].f = NULL;
}

static void do_byteswap(uint8_t *buffer, int size)
{
	int c;

	for (c = 0; c < size; c += 2)
	{
		uint8_t temp = buffer[c];
		buffer[c] = buffer[c+1];
		buffer[c+1] = temp;
	}
}


static void upsample_track(uint8_t *data, int size)
{
	int c;

	for (c = size-1; c >= 0; c--)
	{
		uint8_t new_data = 0;

		if (data[c] & 0x08)
			new_data |= 0x80;
		if (data[c] & 0x04)
			new_data |= 0x20;
		if (data[c] & 0x02)
			new_data |= 0x08;
		if (data[c] & 0x01)
			new_data |= 0x02;
		data[c*2+1] = new_data;

		new_data = 0;
		if (data[c] & 0x80)
			new_data |= 0x80;
		if (data[c] & 0x40)
			new_data |= 0x20;
		if (data[c] & 0x20)
			new_data |= 0x08;
		if (data[c] & 0x10)
			new_data |= 0x02;
		data[c*2] = new_data;
	}
}

static void fdi_seek(int drive, int track)
{
	mfm_t *mfm = &fdi[drive].mfm;
	int c;

	if (!fdi[drive].f)
		return;
//        printf("Track start %i\n",track);
	if (track < 0)
		track = 0;
	if (track > fdi[drive].lasttrack)
		track = fdi[drive].lasttrack - 1;
	c = fdi2raw_loadtrack(fdi[drive].h, (uint16_t *)mfm->track_data[0], (uint16_t *)fdi_timing, track << fdi[drive].sides, &mfm->track_len[0], &mfm->track_index[0], NULL, 1);
	if (!c)
		memset(mfm->track_data[0], 0, 65536);
	if (fdi[drive].sides)
	{
		c = fdi2raw_loadtrack(fdi[drive].h, (uint16_t *)mfm->track_data[1], (uint16_t *)fdi_timing, (track << fdi[drive].sides) + 1, &mfm->track_len[1], &mfm->track_index[1], NULL, 1);
		if (!c)
			memset(mfm->track_data[1], 0, 65536);
	}
	else
	{
		memset(mfm->track_data[1], 0, 65536);
		mfm->track_len[1]   = 10000;
		mfm->track_index[1] = 100;
	}

	do_byteswap(mfm->track_data[0], (mfm->track_len[0] + 7) / 8);
	do_byteswap(mfm->track_data[1], (mfm->track_len[1] + 7) / 8);

	upsample_track(mfm->track_data[0], (mfm->track_len[0] + 7) / 8);
	upsample_track(mfm->track_data[1], (mfm->track_len[1] + 7) / 8);
	mfm->track_len[0] *= 2;
	mfm->track_len[1] *= 2;
	mfm->track_index[0] *= 2;
	mfm->track_index[1] *= 2;

//        rpclog("SD Track %i Len %i Index %i %i\n", track, mfm->track_len[0][0], mfm->track_index[0][0],c);
//        rpclog("DD Track %i Len %i Index %i %i\n", track, mfm->track_len[0][1], mfm->track_index[0][1],c);
}

static void fdi_readsector(int drive, int sector, int track, int side, int density)
{
	fdi_drive = drive;
	mfm_readsector(&fdi[drive].mfm, drive, sector, track, side, density);
}

static void fdi_writesector(int drive, int sector, int track, int side, int density)
{
	fdi_drive = drive;
	mfm_writesector(&fdi[drive].mfm, drive, sector, track, side, density);
}

static void fdi_readaddress(int drive, int track, int side, int density)
{
	fdi_drive = drive;
	mfm_readaddress(&fdi[drive].mfm, drive, track, side, density);
}

static void fdi_format(int drive, int track, int side, int density)
{
	fdi_drive = drive;
	mfm_format(&fdi[drive].mfm, drive, track, side, density);
}

static void fdi_stop()
{
	mfm_stop(&fdi[fdi_drive].mfm);
}

static void fdi_poll()
{
	mfm_common_poll(&fdi[fdi_drive].mfm);
}

static disc_funcs_t fdi_disc_funcs =
{
	.seek        = fdi_seek,
	.readsector  = fdi_readsector,
	.writesector = fdi_writesector,
	.readaddress = fdi_readaddress,
	.poll        = fdi_poll,
	.format      = fdi_format,
	.stop        = fdi_stop,
	.close       = fdi_close
};
