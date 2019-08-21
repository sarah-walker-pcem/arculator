#include "podule_api.h"

void *midi_init(void *p, void (*receive)(void *p, uint8_t val), void (*log)(const char *format, ...), const podule_callbacks_t *podule_callbacks, podule_t *podule);
void midi_close(void *p);
void midi_write(void *p, uint8_t val);

podule_config_selection_t *midi_out_devices_config(void);
podule_config_selection_t *midi_in_devices_config(void);
