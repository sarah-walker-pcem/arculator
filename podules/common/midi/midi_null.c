#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "podule_api.h"
#include "midi.h"

typedef struct midi_t
{
        void *p;
} midi_t;

void *midi_init(void *p, void (*receive)(void *p, uint8_t val), void (*log)(const char *format, ...), const podule_callbacks_t *podule_callbacks, podule_t *podule)
{
        midi_t *midi;

        midi = malloc(sizeof(midi_t));
        memset(midi, 0, sizeof(midi_t));
        
        midi->p = p;

        return midi;
}

void midi_close(void *p)
{
        midi_t *midi = p;

        free(midi);
}

void midi_write(void *p, uint8_t val)
{
}

podule_config_selection_t *midi_out_devices_config(void)
{
        int nr_devs;
        podule_config_selection_t *sel;
        podule_config_selection_t *sel_p;
        char *midi_dev_text = malloc(65536);
        int c;

        nr_devs = 0;
        sel = malloc(sizeof(podule_config_selection_t) * (nr_devs+2));
        sel_p = sel;

        strcpy(midi_dev_text, "None");
        sel_p->description = midi_dev_text;
        sel_p->value = -1;
        sel_p++;
        midi_dev_text += strlen(midi_dev_text)+1;

        strcpy(midi_dev_text, "");
        sel_p->description = midi_dev_text;

        return sel;
}
podule_config_selection_t *midi_in_devices_config(void)
{
        int nr_devs;
        podule_config_selection_t *sel;
        podule_config_selection_t *sel_p;
        char *midi_dev_text = malloc(65536);
        int c;

        nr_devs = 0;
        sel = malloc(sizeof(podule_config_selection_t) * (nr_devs+2));
        sel_p = sel;

        strcpy(midi_dev_text, "None");
        sel_p->description = midi_dev_text;
        sel_p->value = -1;
        sel_p++;
        midi_dev_text += strlen(midi_dev_text)+1;

        strcpy(midi_dev_text, "");
        sel_p->description = midi_dev_text;

        return sel;
}
