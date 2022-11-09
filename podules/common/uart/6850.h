typedef struct m6850_t
{
	uint8_t ctrl, status;

	int tx_irq_pending;

	uint8_t rx_queue[256];
	int rx_rp, rx_wp;
	int rx_pending;
	uint8_t rx_data;

	int input_clock;
	int baud_rate;

	void (*set_irq)(void *p, int state);
	void (*tx_data)(void *p, uint8_t val);
	void (*log)(const char *format, ...);
	void *p;
} m6850_t;

void m6850_init(m6850_t *m6850, int input_clock, void (*set_irq)(void *p, int state), void (*tx_data)(void *p, uint8_t val), void *p, void (*log)(const char *format, ...));
uint8_t m6850_read(m6850_t *m6850, uint32_t addr);
void m6850_write(m6850_t *m6850, uint32_t addr, uint8_t val);
void m6850_run(m6850_t *m6850, int timeslice_us);

void m6850_receive(m6850_t *m6850, uint8_t val);
