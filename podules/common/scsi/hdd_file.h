typedef struct hdd_file_t
{
	FILE *f;
	int sectors;
} hdd_file_t;

void hdd_load(hdd_file_t *hdd, const char *fn, int sectors);
void hdd_close(hdd_file_t *hdd);
int hdd_read_sectors(hdd_file_t *hdd, int offset, int nr_sectors, void *buffer);
int hdd_write_sectors(hdd_file_t *hdd, int offset, int nr_sectors, void *buffer);
int hdd_format_sectors(hdd_file_t *hdd, int offset, int nr_sectors);
