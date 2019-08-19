#include "podule_api.h"

void *sound_out_init(void *p, int freq, int buffer_size, void (*log)(const char *format, ...), const podule_callbacks_t *podule_callbacks, podule_t *podule);
void sound_out_close(void *p);
void sound_out_buffer(void *p, int16_t *buffer, int len);
