void adf_init();

void adf_load(int drive, char *fn);
void adl_load(int drive, char *fn);
void adf_arcdd_load(int drive, char *fn);
void adf_archd_load(int drive, char *fn);
void adf_loadex(int drive, char *fn, int sectors, int size, int dblside, int dblstep, int density);
