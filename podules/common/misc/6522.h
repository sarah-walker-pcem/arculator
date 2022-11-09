typedef struct via6522_t
{
	uint8_t  ora,   orb,   ira,   irb;
	uint8_t  ddra,  ddrb;
	uint8_t  sr;
	uint32_t t1l,   t2l;
	int      t1c,   t2c;
	uint8_t  acr,   pcr,   ifr,   ier;
	int      t1hit, t2hit;
	int      ca1,   ca2,   cb1,   cb2;
	int      intnum;

	uint8_t  (*read_portA)(void *p);
	uint8_t  (*read_portB)(void *p);
	void     (*write_portA)(void *p, uint8_t val);
	void     (*write_portB)(void *p, uint8_t val);

	void     (*set_ca1)(void *p, int level);
	void     (*set_ca2)(void *p, int level);
	void     (*set_cb1)(void *p, int level);
	void     (*set_cb2)(void *p, int level);

	void (*set_irq)(void *p, int state);
	void *p;
} via6522_t;

void    via6522_init(via6522_t *v, void (*set_irq)(void *p, int state), void *p);
uint8_t via6522_read(via6522_t *v, uint16_t addr);
void    via6522_write(via6522_t *v, uint16_t addr, uint8_t val);
void    via6522_updatetimers(via6522_t *v, int time);

void via6522_set_ca1(via6522_t *v, int level);
void via6522_set_ca2(via6522_t *v, int level);
void via6522_set_cb1(via6522_t *v, int level);
void via6522_set_cb2(via6522_t *v, int level);
