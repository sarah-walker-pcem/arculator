extern uint64_t mem_speed[16384][2];

void mem_setromspeed(int n, int s);
void mem_updatetimings();

uint32_t readmemf_debug(uint32_t a);

extern int mem_dorefresh;
extern uint64_t mem_spd_multi;

extern uint8_t *rom_5th_column;
extern uint8_t *rom_arcrom;
