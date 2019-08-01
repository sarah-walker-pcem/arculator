void c82c711_fdc_reset(void);
void c82c711_fdc_init(void);

void c82c711_fdc_write(uint16_t addr, uint8_t val);
uint8_t c82c711_fdc_read(uint16_t addr);

uint8_t c82c711_fdc_dmaread(uint32_t addr);
void c82c711_fdc_dmawrite(uint32_t addr, uint8_t val);
