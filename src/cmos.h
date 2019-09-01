void cmos_init();
void cmos_load();
void cmos_save();

void i2c_change(int new_clock, int new_data);

extern int cmos_changed;
extern int i2c_clock, i2c_data;

#define CMOS_CHANGE_DELAY 10 /*10cs = 100ms*/
