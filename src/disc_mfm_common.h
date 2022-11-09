typedef struct mfm_t
{
	uint8_t track_data[2][65536]; /*[side][byte]*/
	int track_len[3];
	int track_index[3];

	int sector, track, side, drive, density;

	int in_read, in_write, in_readaddr, in_format;
	int sync_required;
	uint64_t buffer;
	int pos, revs;
	int indextime_blank;
	int pollbytesleft, pollbitsleft, ddidbitsleft;
	int readidpoll, readdatapoll, writedatapoll;
	int rw_write;
	int nextsector;
	uint8_t sectordat[1026];
	uint16_t crc;
	int lastdat[2], sectorcrc[2];
	int sectorsize, fdc_sectorsize;
	int last_bit;

	int write_protected;
	void (*writeback)(int drive);
} mfm_t;

void mfm_init(void);
void mfm_common_poll(mfm_t *mfm);
void mfm_readsector(mfm_t *mfm, int drive, int sector, int track, int side, int density);
void mfm_writesector(mfm_t *mfm, int drive, int sector, int track, int side, int density);
void mfm_readaddress(mfm_t *mfm, int drive, int track, int side, int density);
void mfm_format(mfm_t *mfm, int drive, int track, int side, int density);
void mfm_stop(mfm_t *mfm);

void mfm_index(mfm_t *mfm, int is_blank_track);
void mfm_process_read_bit(mfm_t *mfm, uint16_t new_data);
