typedef struct d7002c_t
{
	uint8_t status;
	uint8_t high;
	uint8_t low;
	uint8_t latch;

	int time;

	void (*set_irq)(void *p, int state);
	void *p;
} d7002c_t;

void d7002c_init(d7002c_t *d7002c, void (*set_irq)(void *p, int state), void *p);
void d7002c_poll(d7002c_t *d7002c);
uint8_t d7002c_read(d7002c_t *d7002c, uint16_t addr);
void d7002c_write(d7002c_t *d7002c, uint16_t addr, uint8_t val);
