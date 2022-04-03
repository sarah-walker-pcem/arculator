void *ne2000_init();
void ne2000_close(void *p);
void ne2000_do_reset(void *p);
uint8_t ne2000_read(uint16_t address, void *p);
void ne2000_write(uint16_t address, uint8_t value, void *p);
uint16_t ne2000_dma_read_w(uint16_t offset, void *p);
void ne2000_dma_write_w(uint16_t offset, uint16_t value, void *p);
void ne2000_poll(void *p);