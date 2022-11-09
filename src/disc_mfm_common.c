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
	mfm->sync_required = (density != 2);
}

void mfm_writesector(mfm_t *mfm, int drive, int sector, int track, int side, int density)
{
	mfm->revs = 0;
	mfm->sector = sector;
	mfm->track  = track;
	mfm->side   = side;
	mfm->drive  = drive;
	rpclog("Write sector %i %i %i %i %i\n",drive,side,track,sector,density);

	mfm->in_write = 1;
	mfm->sync_required = (density != 2);
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
	mfm->sync_required = (density != 2);
}

void mfm_format(mfm_t *mfm, int drive, int track, int side, int density)
{
	mfm->revs = 0;
	mfm->track   = track;
	mfm->side    = side;
	mfm->density = density;
	mfm->drive   = drive;
//        printf("Format %i %i %i\n",drive,side,track);

	mfm->in_format = 1;
	mfm->sync_required = 0;
}

void mfm_stop(mfm_t *mfm)
{
	mfm->in_read = mfm->in_write = mfm->in_readaddr = mfm->in_format = 0;
	mfm->nextsector = mfm->ddidbitsleft = mfm->pollbitsleft = 0;
}


static void calccrc(mfm_t *mfm, uint8_t byte)
{
	mfm->crc = (mfm->crc << 8) ^ CRCTable[(mfm->crc >> 8) ^ byte];
}

static uint16_t pack_2us(uint32_t in_data)
{
	uint16_t out_data = 0;
	int c;

	for (c = 0; c < 16; c++)
	{
		if (in_data & (2 << c*2))
			out_data |= (1 << c);
	}

	return out_data;
}
static uint16_t pack_4us(uint64_t in_data)
{
	uint16_t out_data = 0;
	int c;

	for (c = 0; c < 16; c++)
	{
		if (in_data & (8ull << c*4))
			out_data |= (1 << c);
	}

	return out_data;
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

static uint64_t encode_fm_4us(mfm_t *mfm, uint8_t data)
{
	uint64_t fm_data = 0x8080808080808080ull; /*Clock bits*/
	int c;

	for (c = 7; c >= 0; c--)
	{
		if (data & (1 << c))
			fm_data |= 8ull << (c*8);
	}

	return fm_data;
}

static uint32_t encode_mfm_2us(mfm_t *mfm, uint8_t data)
{
	uint32_t mfm_data = 0;
	int c;

	for (c = 7; c >= 0; c--)
	{
		if (data & (1 << c))
			mfm_data |= 2 << (c*4);
		else if (!mfm->last_bit)
			mfm_data |= 8 << (c*4);

		mfm->last_bit = data & (1 << c);
	}

	return mfm_data;
}

static uint16_t encode_mfm_1us(mfm_t *mfm, uint8_t data)
{
	uint16_t mfm_data = 0;
	int c;

	for (c = 7; c >= 0; c--)
	{
		if (data & (1 << c))
			mfm_data |= 1 << (c*2);
		else if (!mfm->last_bit)
			mfm_data |= 2 << (c*2);

		mfm->last_bit = data & (1 << c);
	}

	return mfm_data;
}

void mfm_index(mfm_t *mfm, int is_blank_track)
{
	if (!is_blank_track)
	{
		fdc_funcs->indexpulse(fdc_p);
		if (mfm->in_read || mfm->in_readaddr || mfm->in_write)
			mfm->revs++;
	}
	else
	{
		mfm->indextime_blank--;
		if (mfm->indextime_blank <= 0)
		{
			mfm->indextime_blank = 6250 * 8;
			fdc_funcs->indexpulse(fdc_p);
			if (mfm->in_read || mfm->in_readaddr || mfm->in_write)
				mfm->revs++;
		}
	}
	if ((mfm->in_read || mfm->in_readaddr || mfm->in_write) && mfm->revs == 3)
	{
//		rpclog("hfe_poll: Not found!\n");
		fdc_funcs->notfound(fdc_p);
		mfm->in_read = mfm->in_readaddr = mfm->in_write = 0;
	}
}

static void next_bit(mfm_t *mfm)
{
	mfm->pos++;

	if (mfm->pos >= mfm->track_len[mfm->side])
	{
//                rpclog("disc loop %i  index %i  side %i  %p\n", mfm->pos,mfm->track_index[mfm->side], mfm->side, mfm);
		mfm->pos = 0;
	}

	if (mfm->pos == mfm->track_index[mfm->side])
	{
//                rpclog("disc index %i  %i\n", mfm->pos, mfm->indextime_blank);
		mfm_index(mfm, !mfm->track_len[mfm->side]);
	}
}

void mfm_process_read_bit(mfm_t *mfm, uint16_t new_data)
{
	int c;

	if (!mfm->in_read && !mfm->in_readaddr && !mfm->in_write)
		return;

	if (mfm->pollbitsleft)
		mfm->pollbitsleft--;
	if (!mfm->pollbitsleft && mfm->pollbytesleft)
	{
		mfm->pollbytesleft--;
		if (mfm->pollbytesleft)
			mfm->pollbitsleft = 16; /*Set up another word if we need it*/
		if (mfm->readidpoll)
		{
			mfm->sectordat[5 - mfm->pollbytesleft] = decodefm(new_data);
			if (mfm->in_readaddr && !fdc_funcs->sectorid)// && mfm->pollbytesleft > 1)
			{
//                                rpclog("inreadaddr - %02X\n", hfe_sectordat[5 - mfm->pollbytesleft]);
				fdc_funcs->data(mfm->sectordat[5 - mfm->pollbytesleft], fdc_p);
			}
			if (!mfm->pollbytesleft)
			{
//                                rpclog("Sector - %i %i\n", mfm->sectordat[2], mfm->sectordat[0]);
				if ((mfm->sectordat[0] == mfm->track && mfm->sectordat[2] == mfm->sector) || mfm->in_readaddr)
				{
					mfm->crc = (mfm->density) ? 0xcdb4 : 0xffff;
					calccrc(mfm, 0xFE);
					for (c = 0; c < 4; c++)
						calccrc(mfm, mfm->sectordat[c]);
					if ((mfm->crc >> 8) != mfm->sectordat[4] || (mfm->crc & 0xFF) != mfm->sectordat[5])
					{
//                                                rpclog("Header CRC error : %02X %02X %02X %02X\n",mfm->crc>>8,mfm->crc&0xFF,mfm->sectordat[4],mfm->sectordat[5]);
						if (mfm->in_readaddr)
						{
//                                                rpclog("inreadaddr - %02X\n", mfm->sector);
							if (fdc_funcs->sectorid)
								fdc_funcs->sectorid(mfm->sectordat[0], mfm->sectordat[1], mfm->sectordat[2], mfm->sectordat[3], mfm->sectordat[4], mfm->sectordat[5], fdc_p);
							else
								fdc_funcs->finishread(fdc_p);
						}
						else
							mfm->readidpoll = 0;
					}
					else if (mfm->sectordat[0] == mfm->track && mfm->sectordat[2] == mfm->sector && (mfm->in_read || mfm->in_write) && !mfm->in_readaddr)
					{
//                                                rpclog("  read this sector\n");
//						rpclog("      ID %02x %02x %02x %02x %02x %02x\n", mfm->sectordat[0], mfm->sectordat[1], mfm->sectordat[2], mfm->sectordat[3], mfm->sectordat[4], mfm->sectordat[5]);
						mfm->nextsector = 1;
						mfm->readidpoll = 0;
						mfm->sectorsize = (1 << (mfm->sectordat[3] + 7)) + 2;
						mfm->fdc_sectorsize = mfm->sectordat[3];
					}
					else if (mfm->in_readaddr)
					{
//                                                rpclog("hfe_poll: ID %02x %02x %02x %02x %02x %02x\n", mfm->sectordat[0], mfm->sectordat[1], mfm->sectordat[2], mfm->sectordat[3], mfm->sectordat[4], mfm->sectordat[5]);
						if (fdc_funcs->sectorid)
							fdc_funcs->sectorid(mfm->sectordat[0], mfm->sectordat[1], mfm->sectordat[2], mfm->sectordat[3], mfm->sectordat[4], mfm->sectordat[5], fdc_p);
						else
							fdc_funcs->finishread(fdc_p);
						mfm->in_readaddr = 0;
					}
				}
			}
		}
		if (mfm->readdatapoll)
		{
//                        rpclog("data %i %04x\n", mfm->pollbytesleft, new_data);
			if (mfm->pollbytesleft > 1)
			{
				calccrc(mfm, decodefm(new_data));
			}
			else
				mfm->sectorcrc[1 - mfm->pollbytesleft] = decodefm(new_data);
			if (!mfm->pollbytesleft)
			{
				mfm->in_read = 0;
				if ((mfm->crc >> 8) != mfm->sectorcrc[0] || (mfm->crc & 0xFF) != mfm->sectorcrc[1])// || (hfetrack==79 && hfesect==4 && fdc_side&1))
				{
//                                        rpclog("Data CRC error : %02X %02X %02X %02X\n",mfm->crc>>8,mfm->crc&0xFF,mfm->sectorcrc[0],mfm->sectorcrc[1]);
					fdc_funcs->data(decodefm(mfm->lastdat[1]), fdc_p);
					fdc_funcs->finishread(fdc_p);
					fdc_funcs->datacrcerror(fdc_p);
					mfm->readdatapoll = 0;
				}
				else
				{
//                                        rpclog("End of hfe read %02X %02X %02X %02X\n",mfm->crc>>8,mfm->crc&0xFF,mfm->sectorcrc[0],mfm->sectorcrc[1]);
					fdc_funcs->data(decodefm(mfm->lastdat[1]), fdc_p);
					fdc_funcs->finishread(fdc_p);
				}
			}
			else if (mfm->lastdat[1] != 0)
				fdc_funcs->data(decodefm(mfm->lastdat[1]), fdc_p);
			mfm->lastdat[1] = mfm->lastdat[0];
			mfm->lastdat[0] = new_data;
			if (!mfm->pollbytesleft)
				mfm->readdatapoll = 0;
		}
	}

	if (mfm->density && !mfm->readdatapoll)
	{
		if (new_data == 0x4489)
		{
//                        rpclog("Seen 4489 %i\n", mfm->pos);
			mfm->ddidbitsleft = 16;
		}
		else if (mfm->ddidbitsleft)
		{
			mfm->ddidbitsleft--;
			if (!mfm->ddidbitsleft)
			{
//                                rpclog("ID bits over %04X %02X %i\n",new_data,decodefm(new_data),mfm->pos);
				if (decodefm(new_data) == 0xFE)
				{
//                                        rpclog("Sector header\n");
					mfm->pollbytesleft = 6;
					mfm->pollbitsleft  = 16;
					mfm->readidpoll    = 1;
					mfm->nextsector = 0;
				}
				else if (decodefm(new_data) == 0xFB)
				{
					if (mfm->nextsector)
					{
//                                                rpclog("  Starting sector read\n");
						mfm->pollbytesleft  = mfm->sectorsize;
						mfm->pollbitsleft   = 16;
						mfm->readdatapoll   = mfm->in_read;
						mfm->writedatapoll  = mfm->in_write;
						mfm->readidpoll = 0;
						mfm->rw_write = mfm->in_write;
						mfm->nextsector = 0;
						mfm->crc = 0xcdb4;
						if (new_data == 0xF56A)
						{
							calccrc(mfm, 0xF8);
							mfm->last_bit = 0;
						}
						else
						{
							calccrc(mfm, 0xFB);
							mfm->last_bit = 1;
						}
						mfm->lastdat[0] = mfm->lastdat[1] = 0;
					}
				}
			}
		}
	}
	else if (!mfm->readdatapoll)
	{
		if (new_data == 0xF57E)
		{
//                        rpclog("FM sector header %i\n", mfm->pos);
			mfm->pollbytesleft = 6;
			mfm->pollbitsleft  = 16;
			mfm->readidpoll    = 1;
		}
		if (new_data == 0xF56F || new_data == 0xF56A)
		{
//                        rpclog("FM sector data %i  %i\n", mfm->pos, mfm->nextsector);
			if (mfm->nextsector)
			{
				mfm->pollbytesleft  = mfm->sectorsize;
				mfm->pollbitsleft   = 16;
				mfm->readdatapoll   = mfm->in_read;
				mfm->writedatapoll  = mfm->in_write;
				mfm->rw_write = mfm->in_write;
				mfm->nextsector = 0;
				mfm->crc = 0xffff;
				if (new_data == 0xF56A)
					calccrc(mfm, 0xF8);
				else
					calccrc(mfm, 0xFB);
				mfm->lastdat[0] = mfm->lastdat[1] = 0;
			}
		}
	}
}

void mfm_common_poll(mfm_t *mfm)
{
	int tempi, c, polls;
	int nr_bits = 4 >> mfm->density;

	if (mfm->density == 3)
	{
		for (polls = 0; polls < 8; polls++)
			next_bit(mfm);
		return;
	}

	for (polls = 0; polls < 16; polls++)
	{
		uint16_t new_data;

		if (mfm->sync_required)
		{
			if (mfm->density == 0)
			{
				for (c = 0; c < nr_bits; c++)
				{
					int tempi = mfm->track_data[mfm->side][(mfm->pos >> 3) & 0xFFFF] & (1 << (7-(mfm->pos & 7)));
					mfm->buffer <<= 1;
					mfm->buffer |= (tempi ? 1 : 0);
					next_bit(mfm);

					if ((mfm->buffer & 0x8080808080808080ull) == 0x8080808080808080ull &&
					    !(mfm->buffer & 0x7777777777777777ull))
					{
						mfm->sync_required = 0;
						break;
					}
				}
			}
			else if (mfm->density == 1)
			{
				for (c = 0; c < nr_bits; c++)
				{
					int tempi = mfm->track_data[mfm->side][(mfm->pos >> 3) & 0xFFFF] & (1 << (7-(mfm->pos & 7)));
					mfm->buffer <<= 1;
					mfm->buffer |= (tempi ? 1 : 0);
					next_bit(mfm);

					if ((mfm->buffer & 0xffff) == 0x8888)
					{
						mfm->sync_required = 0;
						break;
					}
				}
			}

			if (mfm->sync_required)
				continue;
		}

		if (mfm->in_format)
		{
			mfm->in_format = 0;
			fdc_funcs->writeprotect(fdc_p);
			continue;
		}
		if (mfm->in_write && mfm->write_protected)
		{
			mfm->in_write = 0;
			fdc_funcs->writeprotect(fdc_p);
			continue;
		}

		if (mfm->rw_write)
		{
//                        if (!mfm->writedatapoll)
//                                fatal("rw_write but not writedatapoll\n");
			if (mfm->pollbitsleft == 16)
			{
				uint8_t write_data;

				if (mfm->pollbytesleft <= 2)
				{
					write_data = (mfm->pollbytesleft == 2) ? (mfm->crc >> 8) : (mfm->crc & 0xff);
				}
				else
				{
					int c = fdc_funcs->getdata(mfm->pollbytesleft == 3, fdc_p);

					write_data = c & 0xff;
					calccrc(mfm, write_data);
				}

				if (mfm->density == 2)
					mfm->buffer = (uint64_t)encode_mfm_1us(mfm, write_data) << 48;
				else if (mfm->density == 1)
					mfm->buffer = (uint64_t)encode_mfm_2us(mfm, write_data) << 32;
				else
					mfm->buffer = encode_fm_4us(mfm, write_data);
//                                rpclog("MFM write %04i %02x %016llx   %06i  %04x\n", mfm->pollbytesleft, write_data, mfm->buffer, mfm->pos, mfm->crc);
			}

			for (c = 0; c < nr_bits; c++)
			{
				if (mfm->buffer & (1ull << 63))
					mfm->track_data[mfm->side][(mfm->pos >> 3) & 0xFFFF] |= (1 << (7-(mfm->pos & 7)));
				else
					mfm->track_data[mfm->side][(mfm->pos >> 3) & 0xFFFF] &= ~(1 << (7-(mfm->pos & 7)));
				mfm->buffer <<= 1;
				next_bit(mfm);
			}

			mfm->pollbitsleft--;
			if (!mfm->pollbitsleft)
			{
				mfm->pollbytesleft--;
				if (mfm->pollbytesleft)
					mfm->pollbitsleft = 16; /*Set up another word if we need it*/
				else
				{
//                                        rpclog("MFM finished write\n");
					fdc_funcs->finishread(fdc_p);
					mfm->writedatapoll = 0;
					mfm->rw_write = 0;
					mfm->in_write = 0;
					mfm->writeback(mfm->drive);
				}
			}
		}
		else
		{
			for (c = 0; c < nr_bits; c++)
			{
				tempi = mfm->track_data[mfm->side][(mfm->pos >> 3) & 0xFFFF] & (1 << (7-(mfm->pos & 7)));
				mfm->buffer <<= 1;
				mfm->buffer |= (tempi ? 1 : 0);
				next_bit(mfm);
			}
			if (mfm->density == 2)
				new_data = mfm->buffer & 0xffff;
			else if (mfm->density == 1)
				new_data = pack_2us(mfm->buffer);
			else
				new_data = pack_4us(mfm->buffer);
//                        rpclog("mfm %04x %i    %02x %04x %p\n", new_data, mfm->pos, mfm->track_data[mfm->side][(mfm->pos >> 3) & 0xFFFF], (mfm->pos >> 3) & 0xFFFF, &mfm->track_data[mfm->side][(mfm->pos >> 3) & 0xFFFF]);

			mfm_process_read_bit(mfm, new_data);
		}
	}
}
