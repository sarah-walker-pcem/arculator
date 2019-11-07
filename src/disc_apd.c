/*Arculator 2.0 by Sarah Walker
  APD disc image support*/
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <zlib.h>

#include "arc.h"
#include "disc.h"
#include "disc_apd.h"

static inline unsigned long gzgetil(gzFile *f)
{
        unsigned long temp = gzgetc(*f);
        temp |= (gzgetc(*f) << 8);
        temp |= (gzgetc(*f) << 16);
        temp |= (gzgetc(*f) << 24);
        return temp;
}

static gzFile apd_f[4];
static struct 
{
        struct
        {
        	unsigned long type,rd,len;
        	int pos;
        } track[160],sdtrack[160],hdtrack[160];
} apd_head[4];

static uint8_t apd_trackinfo[4][2][2][65536];
static int apd_sides[4];
static int apd_tracklen[4][2][2];
static int apd_trackindex[4][2][2];
static int apd_lasttrack[4];
static int apd_ds[4];
static int apd_pos;
static int apd_revs;

static int apd_sector, apd_track,   apd_side,    apd_drive, apd_density;
static int apd_inread, apd_inwrite, apd_readpos, apd_inreadaddr;

static uint16_t CRCTable[256];

static int pollbytesleft=0,pollbitsleft=0;

static void apd_setupcrc(uint16_t poly, uint16_t rvalue)
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

void apd_init()
{
//        rpclog("apd reset\n");
        apd_f[0]  = apd_f[1]  = 0;
        apd_ds[0] = apd_ds[1] = 0;
        apd_setupcrc(0x1021, 0xcdb4);
}

void apd_load(int drive, char *fn)
{
        int c;
        int pos = 8 + (166 * 12);
        
        rpclog("apd_load\n");
        
        writeprot[drive] = fwriteprot[drive] = 1;
        apd_f[drive] = gzopen(fn, "rb");
        if (!apd_f[drive]) return;

        gzseek(apd_f[drive], 8, SEEK_SET);

	for (c = 0; c < 160; c++)
	{
                apd_head[drive].sdtrack[c].len = gzgetil(&apd_f[drive]);
	        apd_head[drive].sdtrack[c].rd  = (apd_head[drive].sdtrack[c].len + 7) >> 3;
		apd_head[drive].sdtrack[c].pos = pos;
		pos += apd_head[drive].sdtrack[c].rd;

	        apd_head[drive].track[c].len = gzgetil(&apd_f[drive]);
	        apd_head[drive].track[c].rd  = (apd_head[drive].track[c].len + 7) >> 3;
		apd_head[drive].track[c].pos = pos;
		pos += apd_head[drive].track[c].rd;

	        apd_head[drive].hdtrack[c].len = gzgetil(&apd_f[drive]);
	        apd_head[drive].hdtrack[c].rd  = (apd_head[drive].hdtrack[c].len + 7) >> 3;
		apd_head[drive].hdtrack[c].pos = pos;
		pos += apd_head[drive].hdtrack[c].rd;

		rpclog("Track %i - %i - %i %i %i\n", c, pos, apd_head[drive].track[c].len, apd_head[drive].sdtrack[c].len, apd_head[drive].hdtrack[c].len);
	}
//        if (!apdh[drive]) rpclog("Failed to load!\n");
        apd_lasttrack[drive] = 83;
        apd_sides[drive] = 1;
//        rpclog("Last track %i\n",apdlasttrack[drive]);
        drives[drive].seek        = apd_seek;
        drives[drive].readsector  = apd_readsector;
        drives[drive].writesector = apd_writesector;
        drives[drive].readaddress = apd_readaddress;
        drives[drive].poll        = apd_poll;
        drives[drive].format      = apd_format;
        drives[drive].stop        = apd_stop;
        rpclog("Loaded as apd\n");
}

void apd_close(int drive)
{
        if (apd_f[drive]) gzclose(apd_f[drive]);
        apd_f[drive] = NULL;
}

void apd_seek(int drive, int track)
{
        if (!apd_f[drive]) return;
//        rpclog("Track start %i\n",track);
        if (track < 0) track = 0;
        if (track >= apd_lasttrack[drive]) track = apd_lasttrack[drive] - 1;
        
        track <<= 1;
        
        if (apd_head[drive].sdtrack[track].len)
        {
                gzseek(apd_f[drive], apd_head[drive].sdtrack[track].pos, SEEK_SET);
                gzread(apd_f[drive], apd_trackinfo[drive][0][0], apd_head[drive].sdtrack[track].rd);
        }
        else
                memset(apd_trackinfo[drive][0][0], 0, apd_tracklen[drive][0][0]);

        if (apd_head[drive].sdtrack[track + 1].len)
        {
                gzseek(apd_f[drive], apd_head[drive].sdtrack[track + 1].pos, SEEK_SET);
                gzread(apd_f[drive], apd_trackinfo[drive][1][0], apd_head[drive].sdtrack[track + 1].rd);
        }
        else
                memset(apd_trackinfo[drive][1][0], 0, apd_tracklen[drive][1][0]);
                        
        if (apd_head[drive].track[track].len)
        {
                gzseek(apd_f[drive], apd_head[drive].track[track].pos, SEEK_SET);
                gzread(apd_f[drive], apd_trackinfo[drive][0][1], apd_head[drive].track[track].rd);
        }
        else
                memset(apd_trackinfo[drive][0][1], 0, apd_tracklen[drive][0][1]);

        if (apd_head[drive].track[track + 1].len)
        {
                gzseek(apd_f[drive], apd_head[drive].track[track + 1].pos, SEEK_SET);
                gzread(apd_f[drive], apd_trackinfo[drive][1][1], apd_head[drive].track[track + 1].rd);
        }
        else
                memset(apd_trackinfo[drive][1][1], 0, apd_tracklen[drive][1][1]);
                        

        apd_tracklen[drive][0][0] = apd_head[drive].sdtrack[track].len;
        apd_tracklen[drive][1][0] = apd_head[drive].sdtrack[track + 1].len;
        apd_tracklen[drive][0][1] = apd_head[drive].track[track].len;
        apd_tracklen[drive][1][1] = apd_head[drive].track[track + 1].len;
        apd_trackindex[drive][0][0] = 0;
        apd_trackindex[drive][1][0] = 0;
        apd_trackindex[drive][0][1] = 0;
        apd_trackindex[drive][1][1] = 0;

//        rpclog("SD Track %i Len %i %i\n", track, apd_tracklen[drive][0][0], apd_tracklen[drive][1][0]);
//        rpclog("DD Track %i Len %i %i\n", track, apd_tracklen[drive][0][1], apd_tracklen[drive][1][1]);
}

void apd_writeback(int drive, int track)
{
        return;
}

void apd_readsector(int drive, int sector, int track, int side, int density)
{
        apd_revs = 0;
        apd_sector  = sector;
        apd_track   = track;
        apd_side    = side;
        apd_drive   = drive;
        apd_density = density;
//        rpclog("apd Read sector %i %i %i %i %i\n",drive,side,track,sector, density);
//        if (pollbytesleft)
//                rpclog("In the middle of a sector!\n");

//        if (side == 0 && track == 54 && sector == 4 && density == 1)
//                output = 1;
        apd_inread  = 1;
        apd_readpos = 0;
}

void apd_writesector(int drive, int sector, int track, int side, int density)
{
        apd_revs = 0;
        apd_sector = sector;
        apd_track  = track;
        apd_side   = side;
        apd_drive  = drive;
//        rpclog("Write sector %i %i %i %i\n",drive,side,track,sector);

        apd_inwrite = 1;
        apd_readpos = 0;
}

void apd_readaddress(int drive, int track, int side, int density)
{
        apd_revs = 0;
        apd_track   = track;
        apd_side    = side;
        apd_density = density;
        apd_drive   = drive;
//        rpclog("Read address %i %i %i\n",drive,side,track);

        apd_inreadaddr = 1;
        apd_readpos    = 0;
}

void apd_format(int drive, int track, int side, int density)
{
        apd_revs = 0;
        apd_track   = track;
        apd_side    = side;
        apd_density = density;
        apd_drive   = drive;
//        rpclog("Format %i %i %i\n",drive,side,track);

        apd_inwrite = 1;
        apd_readpos = 0;
}

static uint16_t apd_buffer;
static int readidpoll=0,readdatapoll=0,apd_nextsector=0,inreadop=0;
static uint8_t apd_sectordat[1026];
static int lastapddat[2],sectorcrc[2];
static int sectorsize,fdc_sectorsize;
static int ddidbitsleft=0;

static uint8_t decodefm(uint16_t dat)
{
        uint8_t temp;
        temp = 0;
        if (dat & 0x0001) temp |= 1;
        if (dat & 0x0004) temp |= 2;
        if (dat & 0x0010) temp |= 4;
        if (dat & 0x0040) temp |= 8;
        if (dat & 0x0100) temp |= 16;
        if (dat & 0x0400) temp |= 32;
        if (dat & 0x1000) temp |= 64;
        if (dat & 0x4000) temp |= 128;
        return temp;
}

void apd_stop()
{
        apd_inread = apd_inwrite = apd_inreadaddr = 0;
        apd_nextsector = ddidbitsleft = pollbitsleft = 0;
}

static uint16_t crc;

static void calccrc(uint8_t byte)
{
//	rpclog("calccrc : %02X %04X %02X ", byte, crc, CRCTable[(crc >> 8)^byte]);
	crc = (crc << 8) ^ CRCTable[(crc >> 8)^byte];
//	rpclog("%04X\n", crc);
}

static int apd_indextime_blank = 6250 * 8;

void apd_poll()
{
        int polls;
        
        if (!apd_tracklen[apd_drive][apd_side][apd_density])
        {
                int fake_track_len = 50000 << apd_density;
                
                /*No track data for this density, fake index pulse*/
                apd_pos += 16;
                if (apd_pos >= fake_track_len)
                {
                        fdc_indexpulse();
                        apd_pos = 0;
                }
                return;
        }
        
        for (polls = 0; polls < 16; polls++)
        {
                int tempi, c;
                int index = 0;
                if (apd_pos >= apd_tracklen[apd_drive][apd_side][apd_density])
                {
//                        if (apd_tracklen[apd_drive][apd_side][apd_density])
//                                rpclog("Looping! %i\n",apd_pos);
                        apd_pos = 0;
                        if (apd_tracklen[apd_drive][apd_side][apd_density])
                        {
                                fdc_indexpulse();
//                                rpclog("Index pulse\n");
                        }
                        else
                        {
                                apd_indextime_blank--;
                                if (!apd_indextime_blank)
                                {
                                        apd_indextime_blank = 6250 * 8;
                                        fdc_indexpulse();
//                                        rpclog("Index pulse\n");
                                }
                        }
                        index = 1;
                }
                tempi = apd_trackinfo[apd_drive][apd_side][apd_density][((apd_pos >> 3) & 0xFFFF)] & (1 << (7 - (apd_pos & 7)));
                apd_pos++;
                apd_buffer<<=1;
                apd_buffer|=(tempi?1:0);
//                if (apd_tracklen[apd_drive][apd_side][apd_density])
//                        rpclog("apd_buffer %04X %02X\n", apd_buffer, decodefm(apd_buffer));
                if (apd_inwrite)
                {
                        apd_inwrite=0;
                        fdc_writeprotect();
                        return;
                }
                if (!apd_inread && !apd_inreadaddr)
                        continue;
                if (index)
                {
                        apd_revs++;
                        if (apd_revs == 3)
                        {
                                rpclog("Not found!\n");
                                fdc_notfound();
                                apd_inread = apd_inreadaddr = 0;
                                output = 0;
                                return;
                        }
                }
                if (pollbitsleft)
                {
                        pollbitsleft--;
                        if (!pollbitsleft)
                        {
                                pollbytesleft--;
                                if (pollbytesleft) pollbitsleft = 16; /*Set up another word if we need it*/
                                if (readidpoll)
                                {
                                        apd_sectordat[5 - pollbytesleft] = decodefm(apd_buffer);
                                        if (apd_inreadaddr && !fdc_sectorid)// && pollbytesleft > 1)
                                        {
//                                                rpclog("inreadaddr - %02X\n", apd_sectordat[5 - pollbytesleft]);
                                                fdc_data(apd_sectordat[5 - pollbytesleft]);
                                        }
                                        if (!pollbytesleft)
                                        {
//                                                rpclog("Found sector : %02X %02X %02X %02X\n", apd_sectordat[0], apd_sectordat[1], apd_sectordat[2], apd_sectordat[3]);
                                                if ((apd_sectordat[0] == apd_track && apd_sectordat[2] == apd_sector) || apd_inreadaddr)
                                                {
                                                        crc = (apd_density) ? 0xcdb4 : 0xffff;
                                                        calccrc(0xFE);
                                                        for (c = 0; c < 4; c++) calccrc(apd_sectordat[c]);
                                                        if ((crc >> 8) != apd_sectordat[4] || (crc & 0xFF) != apd_sectordat[5])
                                                        {
                                                                rpclog("Header CRC error : %02X %02X %02X %02X\n",crc>>8,crc&0xFF,apd_sectordat[4],apd_sectordat[5]);
//                                                                dumpregs();
//                                                                exit(-1);
                                                                inreadop = 0;
                                                                if (apd_inreadaddr)
                                                                {
//                                                                        rpclog("inreadaddr - %02X\n", apd_sector);
//                                                                        fdc_data(apd_sector);
                                                                        if (fdc_sectorid)
                                                                           fdc_sectorid(apd_sectordat[0], apd_sectordat[1], apd_sectordat[2], apd_sectordat[3], apd_sectordat[4], apd_sectordat[5]);
                                                                        else
                                                                           fdc_finishread();
                                                                }
                                                                else             fdc_headercrcerror();
                                                                output = 0;
                                                                return;
                                                        }
                                                        if (apd_sectordat[0] == apd_track && apd_sectordat[2] == apd_sector && apd_inread && !apd_inreadaddr)
                                                        {
                                                                apd_nextsector = 1;
                                                                readidpoll = 0;
                                                                sectorsize = (1 << (apd_sectordat[3] + 7)) + 2;
                                                                fdc_sectorsize = apd_sectordat[3];
                                                        }
                                                        if (apd_inreadaddr)
                                                        {
                                                                output = 0;
                                                                if (fdc_sectorid)
                                                                   fdc_sectorid(apd_sectordat[0], apd_sectordat[1], apd_sectordat[2], apd_sectordat[3], apd_sectordat[4], apd_sectordat[5]);
                                                                else
                                                                   fdc_finishread();
                                                                apd_inreadaddr = 0;
                                                        }
                                                }
                                        }
                                }
                                if (readdatapoll)
                                {
                                        if (pollbytesleft > 1)
                                        {
                                                calccrc(decodefm(apd_buffer));
                                        }
                                        else
                                           sectorcrc[1 - pollbytesleft] = decodefm(apd_buffer);
//                                        rpclog("%04i : %02X\n", pollbytesleft, decodefm(lastapddat[1]));
                                        if (!pollbytesleft)
                                        {
                                                apd_inread = 0;
                                                if ((crc >> 8) != sectorcrc[0] || (crc & 0xFF) != sectorcrc[1])// || (apdtrack==79 && apdsect==4 && fdc_side&1))
                                                {
                                                        rpclog("Data CRC error\n");// : %02X %02X %02X %02X %i %04X %02X%02X %i\n",crc>>8,crc&0xFF,sectorcrc[0],sectorcrc[1],apdpos,crc,sectorcrc[0],sectorcrc[1],ftracklen[0][0][apddensity]);
                                                        inreadop = 0;
                                                        fdc_data(decodefm(lastapddat[1]));
                                                        fdc_finishread();
                                                        fdc_datacrcerror();
                                                        readdatapoll = 0;
                                                        output = 0;
                                                        return;
                                                }
//                                                rpclog("End of apd read %02X %02X %02X %02X\n",crc>>8,crc&0xFF,sectorcrc[0],sectorcrc[1]);
                                                fdc_data(decodefm(lastapddat[1]));
                                                fdc_finishread();
                                                output = 0;
                                        }
                                        else if (lastapddat[1] != 0)
                                                fdc_data(decodefm(lastapddat[1]));
                                        lastapddat[1] = lastapddat[0];
                                        lastapddat[0] = apd_buffer;
                                        if (!pollbytesleft)
                                                readdatapoll = 0;
                                }
                        }
                }
                else if (apd_buffer == 0x4489 && apd_density)
                {
//                        rpclog("Found sync\n");
                        ddidbitsleft = 17;
                }
                else if (apd_buffer == 0xF57E && !apd_density)
                {
                        pollbytesleft = 6;
                        pollbitsleft  = 16;
                        readidpoll    = 1;
                }
                else if ((apd_buffer == 0xF56F || apd_buffer == 0xF56A) && !apd_density)
                {
                        if (apd_nextsector)
                        {
                                pollbytesleft  = sectorsize;
                                pollbitsleft   = 16;
                                readdatapoll   = 1;
                                apd_nextsector = 0;
                                crc = 0xffff;
                                if (apd_buffer == 0xF56A) calccrc(0xF8);
                                else                      calccrc(0xFB);
                                lastapddat[0] = lastapddat[1] = 0;
                        }
                }
                if (ddidbitsleft)
                {
                        ddidbitsleft--;
                        if (!ddidbitsleft)
                        {
//                                rpclog("ID bits over %04X %02X %i\n", apd_buffer, decodefm(apd_buffer), apd_pos);
                                if (decodefm(apd_buffer) == 0xFE)
                                {
//                                        rpclog("Sector header\n");
                                        pollbytesleft = 6;
                                        pollbitsleft  = 16;
                                        readidpoll    = 1;
                                }
                                else if (decodefm(apd_buffer) == 0xFB)
                                {
//                                        rpclog("Data header\n");
                                        if (apd_nextsector)
                                        {
                                                pollbytesleft  = sectorsize;
                                                pollbitsleft   = 16;
                                                readdatapoll   = 1;
                                                apd_nextsector = 0;
                                                crc = 0xcdb4;
                                                if (apd_buffer == 0xF56A) calccrc(0xF8);
                                                else                      calccrc(0xFB);
                                                lastapddat[0] = lastapddat[1] = 0;
                                                //output = 1;
                                        }
                                }
                        }
                }
        }
}
