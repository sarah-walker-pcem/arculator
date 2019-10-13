#include <alsa/asoundlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "podule_api.h"
#include "midi.h"

typedef struct midi_t
{
        int pos, len;
        uint32_t command;
        int insysex;
        uint8_t sysex_data[1024+2];
        
        snd_rawmidi_t *out_device;
        snd_rawmidi_t *in_device;
        
        pthread_t in_thread;
        volatile int in_thread_term;

        void (*receive)(void *p, uint8_t val);
        
        void *p;
} midi_t;

static int midi_id;

void midi_close();

#define MAX_MIDI_DEVICES 50
static struct
{
	int card;
	int device;
	int sub;
        int is_input;
        int is_output;
	char name[50];
} midi_devices[MAX_MIDI_DEVICES];

static int midi_device_count = 0;

static int midi_queried = 0;

static void midi_query()
{
	int status;
	int card = -1;

	midi_queried = 1;

	if ((status = snd_card_next(&card)) < 0)
		return;

	if (card < 0)
		return; /*No cards*/

	while (card >= 0)
	{
		char *shortname;

		if ((status = snd_card_get_name(card, &shortname)) >= 0)
		{
			snd_ctl_t *ctl;
			char name[32];

			sprintf(name, "hw:%i", card);

			if ((status = snd_ctl_open(&ctl, name, 0)) >= 0)
			{
				int device = -1;

				do
				{
					status = snd_ctl_rawmidi_next_device(ctl, &device);
					if (status >= 0 && device != -1)
					{
						snd_rawmidi_info_t *info;
						int sub_nr, sub;

						snd_rawmidi_info_alloca(&info);
						snd_rawmidi_info_set_device(info, device);
						snd_ctl_rawmidi_info(ctl, info);
						sub_nr = snd_rawmidi_info_get_subdevices_count(info);
//						printf("output sub_nr=%i\n",sub_nr);

						for (sub = 0; sub < sub_nr; sub++)
						{
							snd_rawmidi_info_set_subdevice(info, sub);

							if (snd_ctl_rawmidi_info(ctl, info) == 0)
							{
                                                                int status;
                                                                
//								printf("%s: MIDI device=%i:%i:%i\n", shortname, card, device,sub);

								midi_devices[midi_device_count].card = card;
								midi_devices[midi_device_count].device = device;
								midi_devices[midi_device_count].sub = sub;
								snprintf(midi_devices[midi_device_count].name, 50, "%s (%i:%i:%i)", shortname, card, device, sub);
                                                                
                                                                snd_rawmidi_info_set_stream(info, SND_RAWMIDI_STREAM_OUTPUT);
                                                                status = snd_ctl_rawmidi_info(ctl, info);
                                                                if (status == 0)
                                                                        midi_devices[midi_device_count].is_output = 1;
                                                                
                                                                snd_rawmidi_info_set_stream(info, SND_RAWMIDI_STREAM_INPUT);
                                                                status = snd_ctl_rawmidi_info(ctl, info);
                                                                if (status == 0)
                                                                        midi_devices[midi_device_count].is_input = 1;
                                                                
                                                                
								midi_device_count++;
								if (midi_device_count >= MAX_MIDI_DEVICES)
									return;
							}
						}
					}
				} while (device >= 0);
			}
		}

		if (snd_card_next(&card) < 0)
			break;
	}
}

void *midi_in_callback(void *p);

void *midi_init(void *p, void (*receive)(void *p, uint8_t val), void (*log)(const char *format, ...), const podule_callbacks_t *podule_callbacks, podule_t *podule)
{
        midi_t *midi;
	char portname[32];        
        char name[256];
        int c;
        const char *device;
        int midi_in_dev_nr, midi_out_dev_nr;
        
    	if (!midi_queried)
                midi_query();

        midi = malloc(sizeof(midi_t));
        memset(midi, 0, sizeof(midi_t));
        
        device = podule_callbacks->config_get_string(podule, "midi_in_device", "0");
        sscanf(device, "%i", &midi_in_dev_nr);
	if (log)
	        log("midi_init: midi_in_dev_nr=%i\n", midi_in_dev_nr);
        device = podule_callbacks->config_get_string(podule, "midi_out_device", "0");
        sscanf(device, "%i", &midi_out_dev_nr);
	if (log)
	        log("midi_init: midi_out_dev_nr=%i\n", midi_out_dev_nr);

        midi->p = p;
        midi->receive = receive;

        
	sprintf(portname, "hw:%i,%i,%i", midi_devices[midi_out_dev_nr].card,
					 midi_devices[midi_out_dev_nr].device,
					 midi_devices[midi_out_dev_nr].sub);
        if (log)
                log("Opening MIDI port %s\n", portname);

	if (snd_rawmidi_open(NULL, &midi->out_device, portname, SND_RAWMIDI_SYNC) < 0)
	{
                if (log)
                        log("Failed to open MIDI out device\n");
        }

	sprintf(portname, "hw:%i,%i,%i", midi_devices[midi_in_dev_nr].card,
					 midi_devices[midi_in_dev_nr].device,
					 midi_devices[midi_in_dev_nr].sub);
        if (log)
                log("Opening MIDI port %s\n", portname);

	if (snd_rawmidi_open(&midi->in_device, NULL, portname, SND_RAWMIDI_NONBLOCK) < 0)
	{
                if (log)
                        log("Failed to open MIDI in device\n");
        }
        else
        {
                pthread_create(&midi->in_thread, NULL, midi_in_callback, midi);
        }

        return midi;
}

void midi_close(void *p)
{
        midi_t *midi = p;

        midi->in_thread_term = 1;
        pthread_join(midi->in_thread, NULL);
        
        if (midi->in_device != NULL)
        {
                snd_rawmidi_close(midi->in_device);
                midi->in_device = NULL;
        }
        if (midi->out_device != NULL)
        {
                snd_rawmidi_close(midi->out_device);
                midi->out_device = NULL;
        }
}

#define IN_SLEEP_TIME 5000 /*5ms*/

void *midi_in_callback(void *p)
{
        midi_t *midi = p;
        
        while (!midi->in_thread_term)
        {
                uint8_t data;
                int status = snd_rawmidi_read(midi->in_device, &data, 1);
                
                if (status >= 0)
                        midi->receive(midi->p, data);
                else
                        usleep(IN_SLEEP_TIME);
        }
}

static int midi_lengths[8] = {3, 3, 3, 3, 2, 2, 3, 1};

void midi_write(void *p, uint8_t val)
{
        midi_t *midi = p;
        
//        lark_log("midi_write: val=%02x\n", val);
        if ((val & 0x80) && !(val == 0xf7 && midi->insysex))
        {
                midi->pos = 0;
                midi->len = midi_lengths[(val >> 4) & 7];
                midi->command = 0;
                if (val == 0xf0)
                        midi->insysex = 1;
        }

        if (midi->insysex)
        {
                midi->sysex_data[midi->pos++] = val;
                
                if (val == 0xf7 || midi->pos >= 1024+2)
                {
                        snd_rawmidi_write(midi->out_device, midi->sysex_data, midi->pos);
                        midi->insysex = 0;
                }
                return;
        }
                        
        if (midi->len)
        {                
                midi->command |= (val << (midi->pos * 8));
                
                midi->pos++;
                
                if (midi->pos == midi->len)
                        snd_rawmidi_write(midi->out_device, &midi->command, midi->len);
        }
}

podule_config_selection_t *midi_out_devices_config(void)
{
        int nr_devs;
        podule_config_selection_t *sel;
        podule_config_selection_t *sel_p;
        char *midi_dev_text = malloc(65536);
        int c;

        if (!midi_queried)
                midi_query();
        
        nr_devs = midi_device_count;
        sel = malloc(sizeof(podule_config_selection_t) * (nr_devs+2));
        sel_p = sel;

        strcpy(midi_dev_text, "None");
        sel_p->description = midi_dev_text;
        sel_p->value = -1;
        sel_p++;
        midi_dev_text += strlen(midi_dev_text)+1;

        for (c = 0; c < nr_devs; c++)
        {
                if (midi_devices[c].is_output)
                {
                        strcpy(midi_dev_text, midi_devices[c].name);
                        sel_p->description = midi_dev_text;
                        sel_p->value = c;
                        sel_p++;

                        midi_dev_text += strlen(midi_dev_text)+1;
                }
        }

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

        if (!midi_queried)
                midi_query();
        
        nr_devs = midi_device_count;
        sel = malloc(sizeof(podule_config_selection_t) * (nr_devs+2));
        sel_p = sel;

        strcpy(midi_dev_text, "None");
        sel_p->description = midi_dev_text;
        sel_p->value = -1;
        sel_p++;
        midi_dev_text += strlen(midi_dev_text)+1;

        for (c = 0; c < nr_devs; c++)
        {
                if (midi_devices[c].is_input)
                {
                        strcpy(midi_dev_text, midi_devices[c].name);
                        sel_p->description = midi_dev_text;
                        sel_p->value = c;
                        sel_p++;

                        midi_dev_text += strlen(midi_dev_text)+1;
                }
        }

        strcpy(midi_dev_text, "");
        sel_p->description = midi_dev_text;

        return sel;
}
