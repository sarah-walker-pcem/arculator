typedef struct eeprom_93c06_t
{
	uint16_t buffer[16];

	int clk;
	int state;
	int nr_bits;
	int addr;
	int data_out;
	int write_enable;
	int dirty;
	uint32_t data;
	uint32_t cmd;
} eeprom_93c06_t;

int eeprom_93c06_update(eeprom_93c06_t *eeprom, int cs, int clk, int di);