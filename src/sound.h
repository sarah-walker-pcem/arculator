void sound_init(void);
void sound_set_period(int period);
void sound_set_clock(int clock_mhz);
void sound_update_filter(void);

extern int sound_gain;
extern int sound_filter;
