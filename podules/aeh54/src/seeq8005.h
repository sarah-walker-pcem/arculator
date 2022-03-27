#include "net.h"

typedef struct seeq8005_t
{
	uint8_t ram[0x10000];

	uint16_t command;
	uint16_t status;
	uint16_t config_1;
	uint16_t config_2;
	uint16_t rx_end_area;
	uint16_t rx_ptr;
	uint16_t tx_ptr;
	uint16_t dma_addr;

	uint16_t tx_end_ptr;

	uint8_t station_addr[6][6];

	void *p;
	net_t *net;

	void (*set_irq)(void *p, int state);
} seeq8005_t;

void seeq8005_init(seeq8005_t *seeq8005, void (*set_irq)(void *p, int state), void *p, net_t *net);
void seeq8005_close(seeq8005_t *seeq8005);
void seeq8005_poll(seeq8005_t *seeq8005);
uint16_t seeq8005_read(seeq8005_t *seeq8005, uint32_t addr);
void seeq8005_write(seeq8005_t *seeq8005, uint32_t addr, uint16_t val);