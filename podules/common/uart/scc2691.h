typedef struct scc2691_t
{
	uint8_t isr, imr;

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
} scc2691_t;

void scc2691_init(scc2691_t *scc2691, int input_clock, void (*set_irq)(void *p, int state), void (*tx_data)(void *p, uint8_t val), void *p, void (*log)(const char *format, ...));
uint8_t scc2691_read(scc2691_t *scc2691, uint32_t addr);
void scc2691_write(scc2691_t *scc2691, uint32_t addr, uint8_t val);
void scc2691_run(scc2691_t *scc2691, int timeslice_us);

void scc2691_receive(scc2691_t *scc2691, uint8_t val);
