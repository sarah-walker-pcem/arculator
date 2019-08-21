#include "podule_api.h"

void *sound_in_init(void *p, void (*sound_in_buffer)(void *p, void *buffer, int samples), void (*log)(const char *format, ...), const podule_callbacks_t *podule_callbacks, podule_t *podule);
void sound_in_close(void *p);
void sound_in_start(void *p);
void sound_in_stop(void *p);
podule_config_selection_t *sound_in_devices_config(void);
