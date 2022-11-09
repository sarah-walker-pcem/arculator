/*Arculator 2.1 by Sarah Walker
  APD disc image support*/
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <zlib.h>

#include "arc.h"
#include "disc.h"
#include "disc_apd.h"
#include "disc_mfm_common.h"

static disc_funcs_t apd_disc_funcs;

static void apd_seek(int drive, int track);

static inline unsigned long gzgetil(gzFile *f)
{
	unsigned long temp = gzgetc(*f);
	temp |= (gzgetc(*f) << 8);
	temp |= (gzgetc(*f) << 16);
	temp |= (gzgetc(*f) << 24);
	return temp;
}

typedef struct apd_t
{
	struct
	{
		struct
		{
			unsigned long type,rd,len;
			int pos;
		} track[160],sdtrack[160],hdtrack[160];
	} header;

	mfm_t mfm;
	gzFile f;

	int lasttrack;
} apd_t;

static apd_t apd[4];

static int apd_drive;

void apd_init()
{
//        rpclog("apd reset\n");
	apd[0].f = apd[1].f = apd[2].f = apd[3].f = 0;
}

void apd_load(int drive, char *fn)
{
	int c;
	int pos = 8 + (166 * 12);

	rpclog("apd_load\n");

	writeprot[drive] = 1;
	apd[drive].f = gzopen(fn, "rb");
	if (!apd[drive].f)
		return;

	gzseek(apd[drive].f, 8, SEEK_SET);

	for (c = 0; c < 160; c++)
	{
		apd[drive].header.sdtrack[c].len = gzgetil(&apd[drive].f);
		apd[drive].header.sdtrack[c].rd  = (apd[drive].header.sdtrack[c].len + 7) >> 3;
		apd[drive].header.sdtrack[c].pos = pos;
		pos += apd[drive].header.sdtrack[c].rd;

		apd[drive].header.track[c].len = gzgetil(&apd[drive].f);
		apd[drive].header.track[c].rd  = (apd[drive].header.track[c].len + 7) >> 3;
		apd[drive].header.track[c].pos = pos;
		pos += apd[drive].header.track[c].rd;

		apd[drive].header.hdtrack[c].len = gzgetil(&apd[drive].f);
		apd[drive].header.hdtrack[c].rd  = (apd[drive].header.hdtrack[c].len + 7) >> 3;
		apd[drive].header.hdtrack[c].pos = pos;
		pos += apd[drive].header.hdtrack[c].rd;

		rpclog("Track %i - %i - %i %i %i\n", c, pos, apd[drive].header.track[c].len, apd[drive].header.sdtrack[c].len, apd[drive].header.hdtrack[c].len);
	}
//        if (!apdh[drive]) rpclog("Failed to load!\n");
	apd[drive].lasttrack = 83;
	apd[drive].mfm.write_protected = 1;
//        rpclog("Last track %i\n",apdlasttrack[drive]);
	drive_funcs[drive] = &apd_disc_funcs;
	rpclog("Loaded as apd\n");

	apd_seek(drive, disc_get_current_track(drive));
}

static void apd_close(int drive)
{
	if (apd[drive].f)
		gzclose(apd[drive].f);
	apd[drive].f = NULL;
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

static void apd_seek(int drive, int track)
{
	mfm_t *mfm = &apd[drive].mfm;

	if (!apd[drive].f)
		return;
//        rpclog("Track start %i\n",track);
	if (track < 0)
		track = 0;
	if (track >= apd[drive].lasttrack)
		track = apd[drive].lasttrack - 1;

	track <<= 1;

	if (apd[drive].header.track[track].len)
	{
		gzseek(apd[drive].f, apd[drive].header.track[track].pos, SEEK_SET);
		gzread(apd[drive].f, mfm->track_data[0], apd[drive].header.track[track].rd);
	}
	else
		memset(mfm->track_data[0], 0, mfm->track_len[0]);

	if (apd[drive].header.track[track + 1].len)
	{
		gzseek(apd[drive].f, apd[drive].header.track[track + 1].pos, SEEK_SET);
		gzread(apd[drive].f, mfm->track_data[1], apd[drive].header.track[track + 1].rd);
	}
	else
		memset(mfm->track_data[1], 0, mfm->track_len[1]);


	mfm->track_len[0] = apd[drive].header.track[track].len;
	mfm->track_len[1] = apd[drive].header.track[track + 1].len;
	mfm->track_index[0] = 0;
	mfm->track_index[1] = 0;

	upsample_track(mfm->track_data[0], (mfm->track_len[0] + 7) / 8);
	upsample_track(mfm->track_data[1], (mfm->track_len[1] + 7) / 8);
	mfm->track_len[0] *= 2;
	mfm->track_len[1] *= 2;

//        rpclog("SD Track %i Len %i %i\n", track, mfm->track_len[0][0], mfm->track_len[1][0]);
//        rpclog("DD Track %i Len %i %i\n", track, mfm->track_len[0][1], mfm->track_len[1][1]);
}

static void apd_readsector(int drive, int sector, int track, int side, int density)
{
	apd_drive = drive;
	mfm_readsector(&apd[drive].mfm, drive, sector, track, side, density);
}

static void apd_writesector(int drive, int sector, int track, int side, int density)
{
	apd_drive = drive;
	mfm_writesector(&apd[drive].mfm, drive, sector, track, side, density);
}

static void apd_readaddress(int drive, int track, int side, int density)
{
	apd_drive = drive;
	mfm_readaddress(&apd[drive].mfm, drive, track, side, density);
}

static void apd_format(int drive, int track, int side, int density)
{
	apd_drive = drive;
	mfm_format(&apd[drive].mfm, drive, track, side, density);
}

static void apd_stop()
{
	mfm_stop(&apd[apd_drive].mfm);
}

static void apd_poll()
{
	mfm_common_poll(&apd[apd_drive].mfm);
}

static disc_funcs_t apd_disc_funcs =
{
	.seek        = apd_seek,
	.readsector  = apd_readsector,
	.writesector = apd_writesector,
	.readaddress = apd_readaddress,
	.poll        = apd_poll,
	.format      = apd_format,
	.stop        = apd_stop,
	.close       = apd_close
};
