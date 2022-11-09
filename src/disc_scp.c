/*Arculator 2.1 by Sarah Walker
  SCP disc image support*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "arc.h"
#include "disc.h"
#include "disc_mfm_common.h"
#include "disc_scp.h"

#define SCP_MAX_REVOLUTIONS 5 /*arbitary limit*/

static disc_funcs_t scp_disc_funcs;

static void scp_seek(int drive, int track);

typedef struct scp_header_t
{
	char id[3];
	uint8_t version;
	uint8_t disk_type;
	uint8_t nr_revolutions;
	uint8_t start_track;
	uint8_t end_track;
	uint8_t flags;
	uint8_t bit_cell_encoding;
	uint8_t nr_heads;
	uint8_t resolution;
	uint32_t checksum;
} scp_header_t;

typedef struct scp_track_t
{
	uint32_t index_time;
	uint32_t track_length;
	uint32_t data_offset;
} scp_track_t;

typedef struct scp_track_data_header_t
{
	uint8_t trk[3];
	uint8_t track_number;
	scp_track_t revolutions[SCP_MAX_REVOLUTIONS];
} scp_track_data_header_t;

typedef struct scp_t
{
	FILE *f;

	mfm_t mfm;

	scp_header_t header;
	uint32_t track_offsets[168];

	int current_track;
	uint16_t data_buffer[2][SCP_MAX_REVOLUTIONS][0x40000];
	int track_length[2][SCP_MAX_REVOLUTIONS];
	int track_pos;
	int revolution;

	int resolution;

	int time_to_next_bit;
	int time_remaining;
	uint16_t data;
} scp_t;

static scp_t scp[4];

void scp_init(void)
{
	memset(scp, 0, sizeof(scp));
}

void scp_load(int drive, char *fn)
{
	scp[drive].f = fopen(fn, "rb");
	if (!scp[drive].f)
		return;
	writeprot[drive] = 1;

	fread(&scp[drive].header, 16, 1, scp[drive].f);

	rpclog("SCP file %s:\n", fn);
	rpclog(" ID=%c%c%c\n", scp[drive].header.id[0], scp[drive].header.id[1], scp[drive].header.id[2]);
	rpclog(" Disk type=%02x\n", scp[drive].header.disk_type);
	rpclog(" Number of revolutions=%02x\n", scp[drive].header.nr_revolutions);
	rpclog(" Start track=%u\n", scp[drive].header.start_track);
	rpclog(" End track=%u\n", scp[drive].header.end_track);
	rpclog(" Flags=%02x\n", scp[drive].header.flags);
	rpclog(" Bit cell encoding=%u\n", scp[drive].header.bit_cell_encoding);
	rpclog(" Number of heads=%u\n", scp[drive].header.nr_heads);
	rpclog(" Resolution=%u\n", scp[drive].header.resolution);

	fread(&scp[drive].track_offsets, 4, 168, scp[drive].f);

	scp[drive].mfm.write_protected = writeprot[drive];
	scp[drive].mfm.writeback = NULL;
	scp[drive].resolution = (scp[drive].header.resolution + 1) * 25;

	drive_funcs[drive] = &scp_disc_funcs;
	rpclog("Loaded as scp\n");

	scp_seek(drive, disc_get_current_track(drive));
}

static void scp_close(int drive)
{
	if (scp[drive].f)
	{
		fclose(scp[drive].f);
		scp[drive].f = NULL;
	}
}

#define SCP_BITCELL_LENGTH 0xa0 //4us at default 25ns resolution - FM single density
#define SCP_BITCELL_LENGTH_NS (SCP_BITCELL_LENGTH * 25)

static void scp_do_read_track(int drive, int track, int side)
{
	mfm_t *mfm = &scp[drive].mfm;
	int file_track = track*2 + side;
	uint32_t track_offset = scp[drive].track_offsets[file_track];
	scp_track_data_header_t track_data_header;

	if (!track_offset)
	{
//		rpclog("scp_do_read_track: blank track %i %i\n", track, file_track);
		for (int rev = 0; rev < MIN(scp[drive].header.nr_revolutions, SCP_MAX_REVOLUTIONS); rev++)
		{
			/*Bodge something resembling a blank track*/
			scp[drive].data_buffer[side][rev][0] = 0xffff;
			scp[drive].track_length[side][rev] = 1;
		}
		memset(mfm->track_data[side], 0, 65536);
		return;
	}

	fseek(scp[drive].f, track_offset, SEEK_SET);
	fread(&track_data_header, sizeof(track_data_header), 1, scp[drive].f);

	for (int rev = 0; rev < MIN(scp[drive].header.nr_revolutions, SCP_MAX_REVOLUTIONS); rev++)
	{
		fseek(scp[drive].f, track_offset + track_data_header.revolutions[rev].data_offset, SEEK_SET);
		fread(scp[drive].data_buffer[side][rev], track_data_header.revolutions[rev].track_length, 2, scp[drive].f);
		scp[drive].track_length[side][rev] = track_data_header.revolutions[rev].track_length;
	}
}

static void scp_seek(int drive, int track)
{
	mfm_t *mfm = &scp[drive].mfm;

//	rpclog("scp_seek: drive=%i track=%i\n", drive, track);
	if (!scp[drive].f)
	{
		memset(mfm->track_data[0], 0, 65536);
		memset(mfm->track_data[1], 0, 65536);
		return;
	}

	if (track < 0)
		track = 0;
	if (track > 81)
		track = 81;

	scp->current_track = track;

	scp_do_read_track(drive, track, 0);
	scp_do_read_track(drive, track, 1);
}

static int scp_drive = 0;

static void scp_readsector(int drive, int sector, int track, int side, int density)
{
	scp_drive = drive;
	mfm_readsector(&scp[drive].mfm, drive, sector, track, side, density);
}

static void scp_writesector(int drive, int sector, int track, int side, int density)
{
	scp_drive = drive;
	mfm_writesector(&scp[drive].mfm, drive, sector, track, side, density);
}

static void scp_readaddress(int drive, int track, int side, int density)
{
	scp_drive = drive;
	mfm_readaddress(&scp[drive].mfm, drive, track, side, density);
}

static void scp_format(int drive, int track, int side, int density)
{
	scp_drive = drive;
	mfm_format(&scp[drive].mfm, drive, track, side, density);
}

static void scp_stop()
{
	mfm_stop(&scp[scp_drive].mfm);
}

static int scp_next_bit_time()
{
	mfm_t *mfm = &scp[scp_drive].mfm;
	uint16_t data;

	scp[scp_drive].track_pos++;
	if (scp[scp_drive].track_pos >= scp[scp_drive].track_length[mfm->side][scp[scp_drive].revolution])
	{
//		rpclog("  scp loop %i %i %i\n", scp[scp_drive].track_pos, scp[scp_drive].track_length[mfm->side][scp[scp_drive].revolution], mfm->side);
		scp[scp_drive].track_pos = 0;
		scp[scp_drive].revolution = (scp[scp_drive].revolution + 1) % MIN(scp[scp_drive].header.nr_revolutions, SCP_MAX_REVOLUTIONS);

		mfm_index(mfm, 0);
	}

	data = scp[scp_drive].data_buffer[mfm->side][scp[scp_drive].revolution][scp[scp_drive].track_pos];
	data = (data >> 8) | (data << 8);
//	if (scp[scp_drive].current_track == 80)
//		rpclog("  %04x %i  (%i/%i)\n", data, data*25, scp[scp_drive].track_pos, scp[scp_drive].track_length[mfm->side][scp[scp_drive].revolution]);
	return data * scp[scp_drive].resolution; //*25;
}

/*32us for double density*/
static void scp_poll()
{
	mfm_t *mfm = &scp[scp_drive].mfm;
	int bitcell_time = SCP_BITCELL_LENGTH_NS >> mfm->density;

	if (mfm->in_format)
	{
		mfm->in_format = 0;
		fdc_funcs->writeprotect(fdc_p);
		return;
	}
	if (mfm->in_write && mfm->write_protected)
	{
		mfm->in_write = 0;
		fdc_funcs->writeprotect(fdc_p);
		return;
	}

	scp[scp_drive].time_remaining += 4000 >> mfm->density; //32000 >> mfm->density; //32000/2;

	if (!mfm->rw_write)
	{
		while (scp[scp_drive].time_remaining > 0)
		{
			scp[scp_drive].data <<= 1;

			if (scp[scp_drive].time_to_next_bit > ((bitcell_time * 3) / 2)) //((bitcell_time * 3) / 2))
			{
				scp[scp_drive].time_remaining -= bitcell_time; //SCP_BITCELL_LENGTH_NS2;
				scp[scp_drive].time_to_next_bit -= bitcell_time; //SCP_BITCELL_LENGTH_NS2;
			}
			else
			{
				scp[scp_drive].data |= 1;
				scp[scp_drive].time_remaining -= scp[scp_drive].time_to_next_bit;
				scp[scp_drive].time_to_next_bit = scp_next_bit_time();
			}
//			if (scp[scp_drive].current_track == 80)
//				rpclog("scp_poll: %i %04x %i %i\n", scp[scp_drive].data & 1, scp[scp_drive].data, scp[scp_drive].track_pos, scp[scp_drive].time_to_next_bit);

			mfm_process_read_bit(mfm, scp[scp_drive].data);
		}
	}
	else
		scp[scp_drive].time_remaining = 0;
}

static disc_funcs_t scp_disc_funcs =
{
	.seek        = scp_seek,
	.readsector  = scp_readsector,
	.writesector = scp_writesector,
	.readaddress = scp_readaddress,
	.poll        = scp_poll,
	.format      = scp_format,
	.stop        = scp_stop,
	.close       = scp_close,
	.high_res_poll = 1
  };
