void keyboard_init();

extern int key_rx_callback;
void key_do_rx_callback();
extern int key_tx_callback;
void key_do_tx_callback();

extern int keyboard_poll_time;
extern int keyboard_poll_count;
