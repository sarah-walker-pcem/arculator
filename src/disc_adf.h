void adf_init();

void adf_load(int drive, char *fn);
void adl_load(int drive, char *fn);
void adf_arcdd_load(int drive, char *fn);
void adf_archd_load(int drive, char *fn);
void adf_loadex(int drive, char *fn, int sectors, int size, int dblside, int dblstep, int density);

void adf_close(int drive);
void adf_seek(int drive, int track);
void adf_readsector(int drive, int sector, int track, int side, int density);
void adf_writesector(int drive, int sector, int track, int side, int density);
void adf_readaddress(int drive, int sector, int side, int density);
void adf_format(int drive, int sector, int side, int density);
void adf_stop();
void adf_poll();
