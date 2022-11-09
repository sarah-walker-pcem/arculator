void c82c711_fdc_init(void);

void c82c711_fdc_write(uint16_t addr, uint8_t val, void *p);
uint8_t c82c711_fdc_read(uint16_t addr, void *p);

uint8_t c82c711_fdc_dmaread(int tc, void *p);
void c82c711_fdc_dmawrite(uint8_t val, int tc, void *p);

void *c82c711_fdc_init_override(void (*fdc_irq)(int state, void *p),
				void (*fdc_index_irq)(void *p),
				void (*fdc_fiq)(int state, void *p), void *p);
