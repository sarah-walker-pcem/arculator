void keyboard_init();
void keyboard_poll();
uint8_t keyboard_read();
void keyboard_write(uint8_t val);

extern int key_rx_callback;
void key_do_rx_callback();
extern int key_tx_callback;
void key_do_tx_callback();

extern int keyboard_poll_time;
extern int keyboard_poll_count;

void doosmouse();
void setmousepos(uint32_t a);
void getunbufmouse(uint32_t a);
void getosmouse();
void setmouseparams(uint32_t a);
