void d71071l_init();
void d71071l_write(uint32_t addr, uint8_t val, podule *p);
uint8_t d71071l_read(uint32_t addr, podule *p);

int dma_read(int channel, podule *p);
int dma_write(int channel, uint8_t val, podule *p);
