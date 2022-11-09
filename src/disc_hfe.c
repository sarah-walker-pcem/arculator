/*Arculator 2.1 by Sarah Walker
  HFE disc image support*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "arc.h"
#include "disc.h"
#include "disc_hfe.h"
#include "disc_mfm_common.h"

static disc_funcs_t hfe_disc_funcs;

static void hfe_seek(int drive, int track);

static void hfe_writeback(int drive);

#define TRACK_ENCODING_ISOIBM_MFM 0x00
#define TRACK_ENCODING_AMIGA_MFM  0x01
#define TRACK_ENCODING_ISOIBM_FM  0x02
#define TRACK_ENCODING_EMU_FM     0x03

typedef struct hfe_header_t
{
	char signature[8]; /*Should be HXCPICFE*/
	uint8_t revision;
	uint8_t nr_of_tracks;
	uint8_t nr_of_sides;
	uint8_t track_encoding;
	uint16_t bitrate;
	uint16_t floppy_rpm;
	uint8_t floppy_interface_mode;
	uint8_t dnu;
	uint16_t track_list_offset;
	uint8_t write_allowed;
	uint8_t single_step;
	uint8_t track0s0_altencoding;
	uint8_t track0s0_encoding;
	uint8_t track0s1_altencoding;
	uint8_t track0s1_encoding;
} hfe_header_t;

typedef struct hfe_track_t
{
	uint16_t offset;
	uint16_t track_len;
} hfe_track_t;

typedef struct hfe_t
{
	hfe_header_t header;
	hfe_track_t *tracks;
	mfm_t mfm;
	FILE *f;
	int current_track;

	int is_v3;
} hfe_t;

static hfe_t hfe[4];

static int hfe_drive;

static int hfe_load_header(hfe_t *hfe, int drive)
{
	hfe_header_t *header = &hfe->header;

	fread(header->signature, 8, 1, hfe->f);
	header->revision = getc(hfe->f);
	header->nr_of_tracks = getc(hfe->f);
	header->nr_of_sides = getc(hfe->f);
	header->track_encoding = getc(hfe->f);
	fread(&header->bitrate, 2, 1, hfe->f);
	fread(&header->floppy_rpm, 2, 1, hfe->f);
	header->floppy_interface_mode = getc(hfe->f);
	header->dnu = getc(hfe->f);
	fread(&header->track_list_offset, 2, 1, hfe->f);
	header->write_allowed = getc(hfe->f);
	header->single_step = getc(hfe->f);
	header->track0s0_altencoding = getc(hfe->f);
	header->track0s0_encoding = getc(hfe->f);
	header->track0s1_altencoding = getc(hfe->f);
	header->track0s1_encoding = getc(hfe->f);

	if (strncmp(header->signature, "HXCPICFE", 8) && strncmp(header->signature, "HXCHFEV3", 8))
	{
		rpclog("HFE signature does not match\n");
		return -1;
	}
	if (header->revision != 0)
	{
		rpclog("HFE revision %i unsupported\n", header->revision);
		return -1;
	}

//        rpclog("HFE: %i tracks, %i sides\n", header->nr_of_tracks, header->nr_of_sides);
//        rpclog("  track_list_offset: %i\n", header->track_list_offset);
	hfe->is_v3 = !strncmp(header->signature, "HXCHFEV3", 8);
	if (hfe->is_v3)
	{
		rpclog("Loading as HFE v3\n");
		writeprot[drive] = 1;
	}
	hfe->tracks = malloc(header->nr_of_tracks * header->nr_of_sides * sizeof(hfe_track_t));
	fseek(hfe->f, header->track_list_offset * 0x200, SEEK_SET);
	fread(hfe->tracks, header->nr_of_tracks * header->nr_of_sides * sizeof(hfe_track_t), 1, hfe->f);

	return 0;
}

void hfe_init()
{
//        printf("hfe reset\n");
	memset(hfe, 0, sizeof(hfe));
}

void hfe_load(int drive, char *fn)
{
	writeprot[drive] = 0;
	memset(&hfe[drive], 0, sizeof(hfe_t));
	hfe[drive].f = fopen(fn, "rb+");
	if (!hfe[drive].f)
	{
		hfe[drive].f = fopen(fn, "rb");
		if (!hfe[drive].f)
			return;
		writeprot[drive] = 1;
	}
	hfe_load_header(&hfe[drive], drive);
	hfe[drive].mfm.write_protected = writeprot[drive];
	hfe[drive].mfm.writeback = hfe_writeback;

	drive_funcs[drive] = &hfe_disc_funcs;
	rpclog("Loaded as hfe\n");

	hfe_seek(drive, disc_get_current_track(drive));
}

static void hfe_close(int drive)
{
	if (hfe[drive].tracks)
	{
		free(hfe[drive].tracks);
		hfe[drive].tracks = NULL;
	}
	if (hfe[drive].f)
	{
		fclose(hfe[drive].f);
		hfe[drive].f = NULL;
	}
}

static void do_bitswap(uint8_t *data, int size)
{
	int c;

	for (c = 0; c < size; c++)
	{
		uint8_t new_val = 0;

		if (data[c] & 0x01)
			new_val |= 0x80;
		if (data[c] & 0x02)
			new_val |= 0x40;
		if (data[c] & 0x04)
			new_val |= 0x20;
		if (data[c] & 0x08)
			new_val |= 0x10;
		if (data[c] & 0x10)
			new_val |= 0x08;
		if (data[c] & 0x20)
			new_val |= 0x04;
		if (data[c] & 0x40)
			new_val |= 0x02;
		if (data[c] & 0x80)
			new_val |= 0x01;

		data[c] = new_val;
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

static void downsample_track(uint8_t *data, int size)
{
	int c;

	for (c = 0; c < size; c++)
	{
		uint8_t new_data = 0;

		if (data[c*2+1] & 0x80)
			new_data |= 0x08;
		if (data[c*2+1] & 0x20)
			new_data |= 0x04;
		if (data[c*2+1] & 0x08)
			new_data |= 0x02;
		if (data[c*2+1] & 0x02)
			new_data |= 0x01;
		if (data[c*2] & 0x80)
			new_data |= 0x80;
		if (data[c*2] & 0x20)
			new_data |= 0x40;
		if (data[c*2] & 0x08)
			new_data |= 0x20;
		if (data[c*2] & 0x02)
			new_data |= 0x10;
		data[c] = new_data;
	}
}

#define HFE_V3_OPCODE_NOP         0xf0
#define HFE_V3_OPCODE_SET_INDEX   0xf1
#define HFE_V3_OPCODE_SET_BITRATE 0xf2
#define HFE_V3_OPCODE_SKIP_BITS   0xf3
#define HFE_V3_OPCODE_RAND        0xf4

static void process_v3_track(mfm_t *mfm, int side)
{
	int length = (mfm->track_len[side] + 7) / 8;
	uint8_t *in_data = malloc(length);
	int wp = 0;
	int next_byte_bitrate = 0;
	int next_byte_skipbits = 0;
	int skipbits = 0;
	int i;

	memcpy(in_data, mfm->track_data[side], length);
	memset(mfm->track_data[side], 0, 65536);

	for (i = 0; i < length; i++)
	{
//		rpclog("%06i: %02x  %i\n", i, in_data[i], wp);
		if (next_byte_skipbits)
		{
			next_byte_skipbits = 0;
			skipbits = in_data[i];
		}
		else if (next_byte_bitrate)
		{
			next_byte_bitrate = 0;
		}
		else if ((in_data[i] & 0xf0) == 0xf0)
		{
			switch (in_data[i])
			{
				case HFE_V3_OPCODE_NOP:
				break;

				case HFE_V3_OPCODE_SET_INDEX:
				break;

				case HFE_V3_OPCODE_SET_BITRATE:
				next_byte_bitrate = 1;
				break;

				case HFE_V3_OPCODE_SKIP_BITS:
				next_byte_skipbits = 1;
				break;

				case HFE_V3_OPCODE_RAND:
				/*Not currently implemented, just add a byte of zeroes*/
				wp += 8;
				break;

				default:
				rpclog("Unknown HFEv3 opcode %02x\n", in_data[i]);
				exit(-1);
			}
		}
		else
		{
			uint8_t data = skipbits ? (in_data[i] << skipbits) : in_data[i];
			int nr_bits = skipbits ? (8 - skipbits) : 8;
			int bit;

			skipbits = 0;

			for (bit = 0; bit < nr_bits; bit++)
			{
				if (data & 0x80)
					mfm->track_data[side][wp >> 3] |= 0x80 >> (wp & 7);
				data <<= 1;
				wp++;
			}
		}
	}

	mfm->track_len[side] = wp;
	//rpclog("Side %i: length %i->%i\n", side, length*8, wp);

	free(in_data);
}

static void hfe_seek(int drive, int track)
{
	hfe_header_t *header = &hfe[drive].header;
	mfm_t *mfm = &hfe[drive].mfm;
	int c;

	if (!hfe[drive].f)
	{
		memset(mfm->track_data[0], 0, 65536);
		memset(mfm->track_data[1], 0, 65536);
		return;
	}
//        printf("Track start %i\n",track);
	if (track < 0)
		track = 0;
	if (track >= header->nr_of_tracks)
		track = header->nr_of_tracks - 1;

	hfe->current_track = track;

//        rpclog("hfe_seek: drive=%i track=%i\n", drive, track);
//        rpclog("  offset=%04x size=%04x\n", hfe[drive].tracks[track].offset, hfe[drive].tracks[track].track_len);
	fseek(hfe[drive].f, hfe[drive].tracks[track].offset * 0x200, SEEK_SET);
//        rpclog("  start=%06x\n", ftell(hfe[drive].f));
	for (c = 0; c < (hfe[drive].tracks[track].track_len/2); c += 0x100)
	{
		fread(&mfm->track_data[0][c], 256, 1, hfe[drive].f);
		fread(&mfm->track_data[1][c], 256, 1, hfe[drive].f);
	}
//        rpclog("  end=%06x\n", ftell(hfe[drive].f));
	mfm->track_index[0] = 0;
	mfm->track_index[1] = 0;
	mfm->track_len[0] = (hfe[drive].tracks[track].track_len*8)/2;
	mfm->track_len[1] = (hfe[drive].tracks[track].track_len*8)/2;
	do_bitswap(mfm->track_data[0], (mfm->track_len[0] + 7) / 8);
	do_bitswap(mfm->track_data[1], (mfm->track_len[1] + 7) / 8);
	if (hfe[drive].is_v3)
	{
		process_v3_track(mfm, 0);
		process_v3_track(mfm, 1);
	}

	if (header->bitrate < 400)
	{
		upsample_track(mfm->track_data[0], (mfm->track_len[0] + 7) / 8);
		upsample_track(mfm->track_data[1], (mfm->track_len[1] + 7) / 8);
		mfm->track_len[0] *= 2;
		mfm->track_len[1] *= 2;
	}

//        rpclog(" SD side 0 Track %i Len %i Index %i\n", track, mfm->track_len[0][0], mfm->track_index[0][0]);
//        rpclog(" SD side 1 Track %i Len %i Index %i\n", track, mfm->track_len[1][0], mfm->track_index[1][0]);
//        rpclog(" DD side 0 Track %i Len %i Index %i\n", track, mfm->track_len[0], mfm->track_index[0]);
//        rpclog(" DD side 1 Track %i Len %i Index %i\n", track, mfm->track_len[1], mfm->track_index[1]);
}

static void hfe_writeback(int drive)
{
	hfe_header_t *header = &hfe[drive].header;
	mfm_t *mfm = &hfe[drive].mfm;
	int track = hfe[drive].current_track;
	uint8_t track_data[2][65536];
	int c;

	if (hfe[drive].is_v3)
		return;

//        rpclog("hfe_writeback: drive=%i track=%i\n", drive, track);

	for (c = 0; c < 2; c++)
	{
		int track_len = mfm->track_len[c];
		memcpy(track_data[c], mfm->track_data[c], (track_len + 7) / 8);

		if (header->bitrate < 400)
		{
			downsample_track(track_data[c], (track_len + 7) / 8);
			track_len /= 2;
		}
		do_bitswap(track_data[c], (track_len + 7) / 8);
	}

	fseek(hfe[drive].f, hfe[drive].tracks[track].offset * 0x200, SEEK_SET);
//        rpclog(" at %06x\n", ftell(hfe[drive].f));
	for (c = 0; c < (hfe[drive].tracks[track].track_len/2); c += 0x100)
	{
		fwrite(&track_data[0][c], 256, 1, hfe[drive].f);
		fwrite(&track_data[1][c], 256, 1, hfe[drive].f);
	}
}

static void hfe_readsector(int drive, int sector, int track, int side, int density)
{
	hfe_drive = drive;
	mfm_readsector(&hfe[drive].mfm, drive, sector, track, side, density);
}

static void hfe_writesector(int drive, int sector, int track, int side, int density)
{
	hfe_drive = drive;
	mfm_writesector(&hfe[drive].mfm, drive, sector, track, side, density);
}

static void hfe_readaddress(int drive, int track, int side, int density)
{
	hfe_drive = drive;
	mfm_readaddress(&hfe[drive].mfm, drive, track, side, density);
}

static void hfe_format(int drive, int track, int side, int density)
{
	hfe_drive = drive;
	mfm_format(&hfe[drive].mfm, drive, track, side, density);
}

static void hfe_stop()
{
	mfm_stop(&hfe[hfe_drive].mfm);
}

static void hfe_poll()
{
	mfm_common_poll(&hfe[hfe_drive].mfm);
}

static disc_funcs_t hfe_disc_funcs =
{
	.seek        = hfe_seek,
	.readsector  = hfe_readsector,
	.writesector = hfe_writesector,
	.readaddress = hfe_readaddress,
	.poll        = hfe_poll,
	.format      = hfe_format,
	.stop        = hfe_stop,
	.close       = hfe_close
};
