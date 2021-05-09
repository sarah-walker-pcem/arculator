/*Arculator 2.0 by Sarah Walker
  HFE disc image support*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "arc.h"
#include "disc.h"
#include "disc_hfe.h"
#include "disc_mfm_common.h"

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
} hfe_t;

static hfe_t hfe[4];

static int hfe_drive;

static int hfe_load_header(hfe_t *hfe)
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

        if (strncmp(header->signature, "HXCPICFE", 8))
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
        writeprot[drive] = fwriteprot[drive] = 1;
        memset(&hfe[drive], 0, sizeof(hfe_t));
        hfe[drive].f = fopen(fn, "rb");
        hfe_load_header(&hfe[drive]);

        drives[drive].seek        = hfe_seek;
        drives[drive].readsector  = hfe_readsector;
        drives[drive].writesector = hfe_writesector;
        drives[drive].readaddress = hfe_readaddress;
        drives[drive].poll        = hfe_poll;
        drives[drive].format      = hfe_format;
        drives[drive].stop        = hfe_stop;
        rpclog("Loaded as hfe\n");
}

void hfe_close(int drive)
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

static void downsample_track(uint8_t *in_data, uint8_t *out_data, int in_size)
{
        int c;

        for (c = 0; c < in_size; c += 2)
        {
                uint8_t fm_data = 0;

                if (in_data[c+1] & 0x03)
                        fm_data |= 0x01;
                if (in_data[c+1] & 0x0c)
                        fm_data |= 0x02;
                if (in_data[c+1] & 0x30)
                        fm_data |= 0x04;
                if (in_data[c+1] & 0xc0)
                        fm_data |= 0x08;
                if (in_data[c] & 0x03)
                        fm_data |= 0x10;
                if (in_data[c] & 0x0c)
                        fm_data |= 0x20;
                if (in_data[c] & 0x30)
                        fm_data |= 0x40;
                if (in_data[c] & 0xc0)
                        fm_data |= 0x80;

                out_data[c/2] = fm_data;
        }
}


void hfe_seek(int drive, int track)
{
        hfe_header_t *header = &hfe[drive].header;
        mfm_t *mfm = &hfe[drive].mfm;
        int c;
        int density_side0, density_side1;

        if (!hfe[drive].f)
        {
                memset(mfm->track_data[0][0], 0, 65536);
                memset(mfm->track_data[1][0], 0, 65536);
                memset(mfm->track_data[0][1], 0, 65536);
                memset(mfm->track_data[1][1], 0, 65536);
                return;
        }
//        printf("Track start %i\n",track);
        if (track < 0)
                track = 0;
        if (track >= header->nr_of_tracks)
                track = header->nr_of_tracks - 1;

//        rpclog("hfe_seek: drive=%i track=%i\n", drive, track);
//        rpclog("  offset=%04x size=%04x\n", hfe[drive].tracks[track].offset, hfe[drive].tracks[track].track_len);
        fseek(hfe[drive].f, hfe[drive].tracks[track].offset * 0x200, SEEK_SET);
//        rpclog("  start=%06x\n", ftell(hfe[drive].f));
        for (c = 0; c < (hfe[drive].tracks[track].track_len/2); c += 0x100)
        {
                fread(&mfm->track_data[0][1][c], 256, 1, hfe[drive].f);
                if (header->nr_of_sides == 2)
                        fread(&mfm->track_data[1][1][c], 256, 1, hfe[drive].f);
                else
                        memset(&mfm->track_data[1][1][c], 0, 256);
        }
//        rpclog("  end=%06x\n", ftell(hfe[drive].f));
        mfm->track_index[0][0] = 1;
        mfm->track_index[1][0] = 1;
        mfm->track_index[0][1] = 1;
        mfm->track_index[1][1] = 1;
        mfm->track_len[0][1] = (hfe[drive].tracks[track].track_len*8)/2;
        mfm->track_len[1][1] = (hfe[drive].tracks[track].track_len*8)/2;
        mfm->track_len[0][0] = mfm->track_len[0][1] / 2;
        mfm->track_len[1][0] = mfm->track_len[1][1] / 2;

        do_bitswap(mfm->track_data[0][1], (mfm->track_len[0][1] + 7) / 8);
        do_bitswap(mfm->track_data[1][1], (mfm->track_len[1][1] + 7) / 8);

        downsample_track(mfm->track_data[0][1], mfm->track_data[0][0], mfm->track_len[0][1]);
        downsample_track(mfm->track_data[1][1], mfm->track_data[1][0], mfm->track_len[1][1]);

        mfm->track_index[0][2] = 1;
        mfm->track_index[1][2] = 1;
        mfm->track_len[0][2] = 0;
        mfm->track_len[1][2] = 0;
        memset(mfm->track_data[0][2], 0, 65536);
        memset(mfm->track_data[1][2], 0, 65536);

//        rpclog(" SD side 0 Track %i Len %i Index %i\n", track, mfm->track_len[0][0], mfm->track_index[0][0]);
//        rpclog(" SD side 1 Track %i Len %i Index %i\n", track, mfm->track_len[1][0], mfm->track_index[1][0]);
//        rpclog(" DD side 0 Track %i Len %i Index %i\n", track, mfm->track_len[0][1], mfm->track_index[0][1]);
//        rpclog(" DD side 1 Track %i Len %i Index %i\n", track, mfm->track_len[1][1], mfm->track_index[1][1]);
}

void hfe_writeback(int drive, int track)
{
        return;
}

void hfe_readsector(int drive, int sector, int track, int side, int density)
{
        hfe_drive = drive;
        mfm_readsector(&hfe[drive].mfm, drive, sector, track, side, density);
}

void hfe_writesector(int drive, int sector, int track, int side, int density)
{
        hfe_drive = drive;
        mfm_writesector(&hfe[drive].mfm, drive, sector, track, side, density);
}

void hfe_readaddress(int drive, int track, int side, int density)
{
        hfe_drive = drive;
        mfm_readaddress(&hfe[drive].mfm, drive, track, side, density);
}

void hfe_format(int drive, int track, int side, int density)
{
        hfe_drive = drive;
        mfm_format(&hfe[drive].mfm, drive, track, side, density);
}

void hfe_stop()
{
        mfm_stop(&hfe[hfe_drive].mfm);
}

void hfe_poll()
{
        mfm_common_poll(&hfe[hfe_drive].mfm);
}
