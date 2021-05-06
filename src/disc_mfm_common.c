/*Common handling for raw FM/MFM bitstreams*/
#include "arc.h"
#include "disc.h"
#include "disc_mfm_common.h"

static uint16_t CRCTable[256];

static void mfm_setupcrc(uint16_t poly, uint16_t rvalue)
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

void mfm_init(void)
{
        mfm_setupcrc(0x1021, 0xcdb4);
}


void mfm_readsector(mfm_t *mfm, int drive, int sector, int track, int side, int density)
{
        mfm->revs = 0;
        mfm->sector  = sector;
        mfm->track   = track;
        mfm->side    = side;
        mfm->drive   = drive;
        mfm->density = density;
        rpclog("mfm Read sector %i %i %i %i %i\n",drive,side,track,sector, density);

        mfm->in_read  = 1;
}

void mfm_writesector(mfm_t *mfm, int drive, int sector, int track, int side, int density)
{
        mfm->revs = 0;
        mfm->sector = sector;
        mfm->track  = track;
        mfm->side   = side;
        mfm->drive  = drive;
//        printf("Write sector %i %i %i %i\n",drive,side,track,sector);

        mfm->in_write = 1;
}

void mfm_readaddress(mfm_t *mfm, int drive, int track, int side, int density)
{
        mfm->revs = 0;
        mfm->track   = track;
        mfm->side    = side;
        mfm->density = density;
        mfm->drive   = drive;
        rpclog("Read address %i %i %i %i\n",drive,side,track,density);

        mfm->in_readaddr = 1;
}

void mfm_format(mfm_t *mfm, int drive, int track, int side, int density)
{
        mfm->revs = 0;
        mfm->track   = track;
        mfm->side    = side;
        mfm->density = density;
        mfm->drive   = drive;
//        printf("Format %i %i %i\n",drive,side,track);

        mfm->in_write = 1;
}

void mfm_stop(mfm_t *mfm)
{
        mfm->in_read = mfm->in_write = mfm->in_readaddr = 0;
        mfm->nextsector = mfm->ddidbitsleft = mfm->pollbitsleft = 0;
}


static void calccrc(mfm_t *mfm, uint8_t byte)
{
	mfm->crc = (mfm->crc << 8) ^ CRCTable[(mfm->crc >> 8) ^ byte];
}

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

void mfm_common_poll(mfm_t *mfm)
{
        int tempi, c, polls;
        for (polls = 0; polls < 16; polls++)
        {
                if (mfm->pos >= mfm->track_len[mfm->side][mfm->density])
                {
                        mfm->pos = 0;
                        if (mfm->track_len[mfm->side][mfm->density])
                                fdc_indexpulse();
                        else
                        {
                                mfm->indextime_blank--;
                                if (!mfm->indextime_blank)
                                {
                                        mfm->indextime_blank = 6250 * 8;
                                        fdc_indexpulse();
                                }
                        }
                }
//                tempi = mfm->track_data[mfm->side][mfm->density][(mfm->pos >> 3) & 0xFFFF] & (1 << (7-(mfm->pos & 7)));
                tempi = mfm->track_data[mfm->side][mfm->density][(mfm->pos >> 3) & 0xFFFF] & (1 << (7-(mfm->pos & 7)));
                mfm->pos++;
                mfm->buffer <<= 1;
                mfm->buffer |= (tempi ? 1 : 0);
                if (mfm->in_write)
                {
                        mfm->in_write=0;
                        fdc_writeprotect();
                        return;
                }
                if (!mfm->in_read && !mfm->in_readaddr) continue;
                if (mfm->pos == mfm->track_index[mfm->side][mfm->density])
                {
                        mfm->revs++;
                        if (mfm->revs == 3)
                        {
//                                rpclog("hfe_poll: Not found!\n");
                                fdc_notfound();
                                mfm->in_read = mfm->in_readaddr = 0;
                                return;
                        }
                }
//                if (hfe_tracklen[hfe_drive][mfm->side][mfm->density] > 1)
//                        rpclog(" %05i: %i %04x %02x\n", mfm->pos, tempi ? 1 : 0, mfm->buffer, decodefm(mfm->buffer));
                if (mfm->pollbitsleft)
                {
                        mfm->pollbitsleft--;
                        if (!mfm->pollbitsleft)
                        {
                                mfm->pollbytesleft--;
                                if (mfm->pollbytesleft)
                                        mfm->pollbitsleft = 16; /*Set up another word if we need it*/
                                if (mfm->readidpoll)
                                {
                                        mfm->sectordat[5 - mfm->pollbytesleft] = decodefm(mfm->buffer);
                                        if (mfm->in_readaddr && !fdc_sectorid)// && mfm->pollbytesleft > 1)
                                        {
//                                                rpclog("inreadaddr - %02X\n", hfe_sectordat[5 - mfm->pollbytesleft]);
                                                fdc_data(mfm->sectordat[5 - mfm->pollbytesleft]);
                                        }
                                        if (!mfm->pollbytesleft)
                                        {
                                                if ((mfm->sectordat[0] == mfm->track && mfm->sectordat[2] == mfm->sector) || mfm->in_readaddr)
                                                {
                                                        mfm->crc = (mfm->density) ? 0xcdb4 : 0xffff;
                                                        calccrc(mfm, 0xFE);
                                                        for (c = 0; c < 4; c++)
                                                                calccrc(mfm, mfm->sectordat[c]);
                                                        if ((mfm->crc >> 8) != mfm->sectordat[4] || (mfm->crc & 0xFF) != mfm->sectordat[5])
                                                        {
//                                                                printf("Header CRC error : %02X %02X %02X %02X\n",crc>>8,crc&0xFF,hfesectordat[4],hfesectordat[5]);
//                                                                dumpregs();
//                                                                exit(-1);
                                                                if (mfm->in_readaddr)
                                                                {
//                                                                        rpclog("inreadaddr - %02X\n", mfm->sector);
//                                                                        fdc_data(mfm->sector);
                                                                        if (fdc_sectorid)
                                                                                fdc_sectorid(mfm->sectordat[0], mfm->sectordat[1], mfm->sectordat[2], mfm->sectordat[3], mfm->sectordat[4], mfm->sectordat[5]);
                                                                        else
                                                                                fdc_finishread();
                                                                }
                                                                else
                                                                        fdc_headercrcerror();
                                                                return;
                                                        }
                                                        if (mfm->sectordat[0] == mfm->track && mfm->sectordat[2] == mfm->sector && mfm->in_read && !mfm->in_readaddr)
                                                        {
                                                                mfm->nextsector = 1;
                                                                mfm->readidpoll = 0;
                                                                mfm->sectorsize = (1 << (mfm->sectordat[3] + 7)) + 2;
                                                                mfm->fdc_sectorsize = mfm->sectordat[3];
                                                        }
                                                        if (mfm->in_readaddr)
                                                        {
//                                                                rpclog("hfe_poll: ID %02x %02x %02x %02x %02x %02x\n", hfe_sectordat[0], hfe_sectordat[1], hfe_sectordat[2], hfe_sectordat[3], hfe_sectordat[4], hfe_sectordat[5]);
                                                                if (fdc_sectorid)
                                                                        fdc_sectorid(mfm->sectordat[0], mfm->sectordat[1], mfm->sectordat[2], mfm->sectordat[3], mfm->sectordat[4], mfm->sectordat[5]);
                                                                else
                                                                        fdc_finishread();
                                                                mfm->in_readaddr = 0;
                                                        }
                                                }
                                        }
                                }
                                if (mfm->readdatapoll)
                                {
                                        if (mfm->pollbytesleft > 1)
                                        {
                                                calccrc(mfm, decodefm(mfm->buffer));
                                        }
                                        else
                                                mfm->sectorcrc[1 - mfm->pollbytesleft] = decodefm(mfm->buffer);
                                        if (!mfm->pollbytesleft)
                                        {
                                                mfm->in_read = 0;
                                                if ((mfm->crc >> 8) != mfm->sectorcrc[0] || (mfm->crc & 0xFF) != mfm->sectorcrc[1])// || (hfetrack==79 && hfesect==4 && fdc_side&1))
                                                {
//                                                        printf("Data CRC error : %02X %02X %02X %02X %i %04X %02X%02X %i\n",crc>>8,crc&0xFF,sectorcrc[0],sectorcrc[1],hfepos,crc,sectorcrc[0],sectorcrc[1],ftracklen[0][0][hfedensity]);
                                                        fdc_data(decodefm(mfm->lastdat[1]));
                                                        fdc_finishread();
                                                        fdc_datacrcerror();
                                                        mfm->readdatapoll = 0;
                                                        return;
                                                }
//                                                printf("End of hfe read %02X %02X %02X %02X\n",crc>>8,crc&0xFF,sectorcrc[0],sectorcrc[1]);
                                                fdc_data(decodefm(mfm->lastdat[1]));
                                                fdc_finishread();
                                        }
                                        else if (mfm->lastdat[1] != 0)
                                                fdc_data(decodefm(mfm->lastdat[1]));
                                        mfm->lastdat[1] = mfm->lastdat[0];
                                        mfm->lastdat[0] = mfm->buffer;
                                        if (!mfm->pollbytesleft)
                                                mfm->readdatapoll = 0;
                                }
                        }
                }
                if (mfm->buffer == 0x4489 && mfm->density)
                {
//                        rpclog("mfm_common_poll: Found sync pos=%i\n", mfm->pos);
                        mfm->ddidbitsleft = 17;
                }

                if (mfm->buffer == 0xF57E && !mfm->density)
                {
                        mfm->pollbytesleft = 6;
                        mfm->pollbitsleft  = 16;
                        mfm->readidpoll    = 1;
                }
                if ((mfm->buffer == 0xF56F || mfm->buffer == 0xF56A) && !mfm->density)
                {
                        if (mfm->nextsector)
                        {
                                mfm->pollbytesleft  = mfm->sectorsize;
                                mfm->pollbitsleft   = 16;
                                mfm->readdatapoll   = 1;
                                mfm->nextsector = 0;
                                mfm->crc = 0xffff;
                                if (mfm->buffer == 0xF56A)
                                        calccrc(mfm, 0xF8);
                                else
                                        calccrc(mfm, 0xFB);
                                mfm->lastdat[0] = mfm->lastdat[1] = 0;
                        }
                }
                if (mfm->ddidbitsleft)
                {
                        mfm->ddidbitsleft--;
                        if (!mfm->ddidbitsleft)
                        {
//                                rpclog("ID bits over %04X %02X %i\n",mfm->buffer,decodefm(mfm->buffer),mfm->pos);
                                if (decodefm(mfm->buffer) == 0xFE)
                                {
//                                        rpclog("Sector header\n");
                                        mfm->pollbytesleft = 6;
                                        mfm->pollbitsleft  = 16;
                                        mfm->readidpoll    = 1;
                                }
                                else if (decodefm(mfm->buffer) == 0xFB)
                                {
//                                        rpclog("Data header\n");
                                        if (mfm->nextsector)
                                        {
                                                mfm->pollbytesleft  = mfm->sectorsize;
                                                mfm->pollbitsleft   = 16;
                                                mfm->readdatapoll   = 1;
                                                mfm->nextsector = 0;
                                                mfm->crc = 0xcdb4;
                                                if (mfm->buffer == 0xF56A)
                                                        calccrc(mfm, 0xF8);
                                                else
                                                        calccrc(mfm, 0xFB);
                                                mfm->lastdat[0] = mfm->lastdat[1] = 0;
                                        }
                                }
                        }
                }
        }
}
