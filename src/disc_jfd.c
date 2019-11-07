/*Arculator 2.0 by Sarah Walker
  JFD disc image support
  Incomplete, and disabled by default*/
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <zlib.h>

#include "arc.h"
#include "disc.h"
#include "disc_jfd.h"

static struct
{
        struct
        {
                uint32_t id;
                uint32_t min_ver;
                uint32_t size;
                uint32_t seq_num;
                uint32_t game_id;
                uint32_t image_ver;
                uint32_t offset_track;
                uint32_t offset_sector;
                uint32_t offset_data;
                uint32_t offset_delta_track;
                uint32_t offset_delta_sector;
                uint32_t offset_delta_data;
        
                char title[256];

                /*2.04+*/        
                uint32_t flags;
                uint8_t fps;
                uint8_t reserved[3];
                uint32_t obey_length;
        } header;

        int num_tracks;
        
        uint32_t track_offset[166]; /*Unlikely to be more than 83 tracks*/
        
        struct
        {
                uint32_t header;
                uint32_t offset;
        } sector_table[2][256];        /*Unlikely to be more than 256 sectors*/
        
        uint8_t sector_data[2][256][1024];
} jfd[4];

enum
{
        JFD_IDLE,
        JFD_FIND_SECTOR,
        JFD_READ_ID,
        JFD_READ_SECTOR
} jfd_state = JFD_IDLE;

enum
{
        CRC_ID_INVALID = 0x10,
        CRC_DATA_INVALID = 0x20
};
        
static gzFile jfd_f[4];

static int jfd_revs;

static int jfd_sector, jfd_track,   jfd_side,    jfd_drive, jfd_density;
static int jfd_inread, jfd_inwrite, jfd_readpos, jfd_inreadaddr;
static int jfd_notfound;

static int jfd_realsector;

static uint16_t CRCTable[256];

static void jfd_setupcrc(uint16_t poly, uint16_t rvalue)
{
	int c = 256, bc;
	uint16_t crctemp;

	while(c--)
	{
		crctemp = c << 8;
		bc = 8;

		while(bc--)
		{
			if(crctemp & 0x8000)
			{
				crctemp = (crctemp << 1) ^ poly;
			}
			else
			{
				crctemp <<= 1;
			}
		}

		CRCTable[c] = crctemp;
	}
}

void jfd_init()
{
//        rpclog("jfd reset\n");
        jfd_f[0]  = jfd_f[1]  = 0;
        jfd_notfound = 0;
        jfd_setupcrc(0x1021, 0xcdb4);
}

void jfd_load(int drive, char *fn)
{
        rpclog("jfd_load\n");
        
        writeprot[drive] = fwriteprot[drive] = 1;
        jfd_f[drive] = gzopen(fn, "rb");
        if (!jfd_f[drive]) return;
        
        gzread(jfd_f[drive], &jfd[drive].header, sizeof(jfd[drive].header));
        
        rpclog("jfd_load : file ID %c%c%c%c\n", jfd[drive].header.id & 0xff, (jfd[drive].header.id >> 8) & 0xff, (jfd[drive].header.id >> 16) & 0xff, jfd[drive].header.id >> 24);
        rpclog("jfd_load : disc title %s\n", jfd[drive].header.title);

        jfd[drive].num_tracks = (jfd[drive].header.offset_sector - jfd[drive].header.offset_track) / 4;
        rpclog("jfd_load : num_tracks %i\n", jfd[drive].num_tracks);
        
        gzseek(jfd_f[drive], jfd[drive].header.offset_track, SEEK_SET);
        gzread(jfd_f[drive], jfd[drive].track_offset, jfd[drive].num_tracks * 4);
        
        drives[drive].seek        = jfd_seek;
        drives[drive].readsector  = jfd_readsector;
        drives[drive].writesector = jfd_writesector;
        drives[drive].readaddress = jfd_readaddress;
        drives[drive].poll        = jfd_poll;
        drives[drive].format      = jfd_format;
        drives[drive].stop        = jfd_stop;
        rpclog("Loaded as jfd\n");
}

void jfd_close(int drive)
{
        if (jfd_f[drive]) gzclose(jfd_f[drive]);
        jfd_f[drive] = NULL;
}

void jfd_seek(int drive, int track)
{
        int head, sector;
        if (!jfd_f[drive]) return;
        
        rpclog("Seek drive %i to track %i\n", drive, track);
        if (jfd[drive].track_offset[track * 2] != 0xffffffff)
        {
                gzseek(jfd_f[drive], jfd[drive].track_offset[track * 2] + jfd[drive].header.offset_sector, SEEK_SET);
                gzread(jfd_f[drive], jfd[drive].sector_table[0], 256 * 8);
        }
        else
                jfd[drive].sector_table[0][0].header = 0xffffffff;  /*Track empty*/

        if (jfd[drive].track_offset[(track * 2) + 1] != 0xffffffff)
        {
                gzseek(jfd_f[drive], jfd[drive].track_offset[(track * 2) + 1] + jfd[drive].header.offset_sector, SEEK_SET);
                gzread(jfd_f[drive], jfd[drive].sector_table[1], 256 * 8);
        }
        else
                jfd[drive].sector_table[1][0].header = 0xffffffff;  /*Track empty*/
        
        /*Calculate timing*/
        /*Track is approx 6.5kb - 6656 bytes, which is 200 ms
          Sector header is 6 bytes + sync + gap, say 16 bytes
          Inter-sector gap, say 32 bytes*/
        for (head = 0; head < 2; head++)
        {
                int offset = 0;
                for (sector = 0; sector < 256; sector++)
                {
                        if (jfd[drive].sector_table[head][sector].header == 0xffffffff)
                                break;
                        
                        if ((jfd[drive].sector_table[head][sector].header >> 24) == 0xff)
                        {
                                int sector_size;
                                
                                jfd[drive].sector_table[head][sector].header &= ~0xff000000;
                                jfd[drive].sector_table[head][sector].header |= offset << 24;
                                
                                sector_size = (128 << (jfd[drive].sector_table[head][sector].header & 0xf)) + 16 + 96;
                                offset += ((sector_size * 200) / 6656);
                        }
                        rpclog("Track %02i Head %i Sector %02i : offset %i header %08X\n", track, head, sector, jfd[drive].sector_table[head][sector].header >> 24, jfd[drive].sector_table[head][sector].header);

                        if (jfd[drive].header.offset_data != 0xffffffff)
                        {
                                gzseek(jfd_f[drive], jfd[drive].sector_table[head][sector].offset + jfd[drive].header.offset_data, SEEK_SET);
                                gzread(jfd_f[drive], jfd[drive].sector_data[head][sector], 128 << (jfd[drive].sector_table[head][sector].header & 3));
                        }
                        else
                                memset(jfd[drive].sector_data[head][sector], 0xff, 1024);
                }
        }
}

void jfd_writeback(int drive, int track)
{
        return;
}

void jfd_readsector(int drive, int sector, int track, int side, int density)
{
        jfd_revs = 0;
        jfd_sector  = sector;
        jfd_track   = track;
        jfd_side    = side;
        jfd_drive   = drive;
        jfd_density = 1 << density;
        rpclog("jfd Read sector %i %i %i %i %i\n",drive,side,track,sector, density);

        if (!jfd_f[drive])
        {
                jfd_notfound = 500;
                return;
        }

        jfd_inread  = 1;
        jfd_readpos = 0;
        jfd_state   = JFD_FIND_SECTOR;
}

void jfd_writesector(int drive, int sector, int track, int side, int density)
{
        jfd_revs = 0;
        jfd_sector = sector;
        jfd_track  = track;
        jfd_side   = side;
        jfd_density = 1 << density;        
        jfd_drive  = drive;
        rpclog("Write sector %i %i %i %i\n",drive,side,track,sector);

        if (!jfd_f[drive])
        {
                jfd_notfound = 500;
                return;
        }
        jfd_inwrite = 1;
        jfd_readpos = 0;
        jfd_state   = JFD_FIND_SECTOR;
}

void jfd_readaddress(int drive, int track, int side, int density)
{
        jfd_revs = 0;
        jfd_track   = track;
        jfd_side    = side;
        jfd_density = 1 << density;
        jfd_drive   = drive;
        rpclog("Read address %i %i %i\n",drive,side,track);

        jfd_inreadaddr = 1;
        jfd_readpos    = 0;
        jfd_state      = JFD_FIND_SECTOR;
}

void jfd_format(int drive, int track, int side, int density)
{
        jfd_revs = 0;
        jfd_track   = track;
        jfd_side    = side;
        jfd_density = 1 << density;
        jfd_drive   = drive;
        rpclog("Format %i %i %i\n",drive,side,track);

        jfd_inwrite = 1;
        jfd_readpos = 0;
}

static int jfd_nextsector=0;
static int ddidbitsleft=0;

void jfd_stop()
{
        jfd_inread = jfd_inwrite = jfd_inreadaddr = 0;
        jfd_nextsector = ddidbitsleft = 0;
        jfd_state = JFD_IDLE;
}

static int jfd_pos_us = 0;

void jfd_poll()
{
        int c;
        int old_pos_us = jfd_pos_us;

        /*32 us*/
        jfd_pos_us += 32;
        if (jfd_pos_us >= 200000)
                jfd_pos_us -= 200000;
                
        if (jfd_pos_us == 191808)
        {
//                rpclog("index pulse\n");
                fdc_indexpulse();
        }
                
        switch (jfd_state)
        {
                case JFD_IDLE:
                break;
                
                case JFD_FIND_SECTOR:
//                rpclog("JFD_FIND_SECTOR %06i\n", jfd_pos_us);
                c = 0;
                while (jfd[jfd_drive].sector_table[jfd_side][c].header != 0xffffffff)
                {
                        uint32_t header = jfd[jfd_drive].sector_table[jfd_side][c].header;
                        int time_us = (header >> 24) * 1000;
                        if (old_pos_us <= time_us && time_us < jfd_pos_us)
                        {
                                rpclog("%06i : Found sector %i %08X %i %i\n", jfd_pos_us, c, header, old_pos_us, time_us);
                                if (jfd_inreadaddr)
                                {
                                        rpclog("jfd_inreadaddr %i %i %i\n", fdc_sectorid, (header >> 16) & 0xf, jfd_density);
                                        if (fdc_sectorid && ((header >> 16) & 0xf) == jfd_density)
                                        {
                                                fdc_sectorid(jfd_track, jfd_side, header >> 8 & 0xff, header & 3, 0, 0);
                                                jfd_inreadaddr = 0;
                                                jfd_state = JFD_IDLE;
                                        }
                                }
                                else if (jfd_inread)
                                {
                                        if (((header >> 8) & 0xff) == jfd_sector &&
                                            ((header >> 16) & 0xf) == jfd_density)
                                        {
                                                if (header & CRC_ID_INVALID)
                                                {
                                                        fdc_headercrcerror();
                                                        jfd_state = JFD_IDLE;
                                                }
                                                else
                                                {
                                                        jfd_state = JFD_READ_SECTOR;
                                                        jfd_realsector = c;
                                                }
                                        }
                                }
                                break;                                
                        }
                        c++;
                }
                break;
                
                case JFD_READ_SECTOR:
//                rpclog("JFD_READ_SECTOR : %04i %02X\n", jfd_readpos, jfd[jfd_drive].sector_data[jfd_side][jfd_realsector][jfd_readpos]);
                fdc_data(jfd[jfd_drive].sector_data[jfd_side][jfd_realsector][jfd_readpos]);
                jfd_readpos++;
                if (jfd_readpos == (128 << (jfd[jfd_drive].sector_table[jfd_side][jfd_realsector].header & 3)))
                {
//                        rpclog("Read %i bytes\n", jfd_readpos);
                        fdc_finishread();
                        if (jfd[jfd_drive].sector_table[jfd_side][jfd_realsector].header & CRC_DATA_INVALID)
                                fdc_datacrcerror();
                        jfd_inread = 0;
                        jfd_state = JFD_IDLE;
                }
                break;
                
                case JFD_READ_ID:
                break;
        }
}
