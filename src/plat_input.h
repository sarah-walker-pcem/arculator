void input_init();
void input_close();

void mouse_poll_host();
void mouse_get_mickeys(int *x, int *y);
int mouse_get_buttons();
void mouse_capture_enable();
void mouse_capture_disable();

void keyboard_poll_host();
extern int key[512];

#include "keyboard_sdl.h"
