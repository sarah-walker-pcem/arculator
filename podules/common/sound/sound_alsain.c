#include <alsa/asoundlib.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "podule_api.h"
#include "sound_in.h"

typedef struct wave_in_t
{
        snd_pcm_t *device;
        
        pthread_t in_thread;
        volatile int in_thread_term;

        int16_t buffer[4800*2];
        
        int running;
        int buffers_pending;

	void (*sound_in_buffer)(void *p, void *buffer, int samples);
        void *p;
} wave_in_t;

void *wave_in_callback(void *p);

#define MAX_IN_DEVICES 50
static struct
{
	int card;
	int device;
	int sub;
	char name[50];
} in_devices[MAX_IN_DEVICES];

static int in_device_count = 0;

static int in_queried = 0;

static void in_query()
{
	int status;
	int card = -1;

	in_queried = 1;

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
					status = snd_ctl_pcm_next_device(ctl, &device);
					if (status >= 0 && device != -1)
					{
						snd_pcm_info_t *info;
						int sub_nr, sub;

						snd_pcm_info_alloca(&info);
						snd_pcm_info_set_device(info, device);
						snd_ctl_pcm_info(ctl, info);
                                                snd_pcm_info_set_stream(info, SND_PCM_STREAM_CAPTURE);
						sub_nr = snd_pcm_info_get_subdevices_count(info);
//						printf("output sub_nr=%i\n",sub_nr);

						for (sub = 0; sub < sub_nr; sub++)
						{
							snd_pcm_info_set_subdevice(info, sub);

							if (snd_ctl_pcm_info(ctl, info) == 0)
							{
                                                                int status;
                                                                
								printf("%s: PCM device=%i:%i:%i\n", shortname, card, device,sub);

								in_devices[in_device_count].card = card;
								in_devices[in_device_count].device = device;
								in_devices[in_device_count].sub = sub;
								snprintf(in_devices[in_device_count].name, 50, "%s (%i:%i:%i)", shortname, card, device, sub);
                                                                
								in_device_count++;
								if (in_device_count >= MAX_IN_DEVICES)
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

void *sound_in_init(void *p, void (*sound_in_buffer)(void *p, void *buffer, int samples), void (*log)(const char *format, ...), const podule_callbacks_t *podule_callbacks, podule_t *podule)
{
        int c;
        const char *device;
        int device_nr = 0;
        unsigned int rate = 48000;
	char portname[32];        
        snd_pcm_hw_params_t *hw_params;
        wave_in_t *wave_in = malloc(sizeof(wave_in_t));
        memset(wave_in, 0, sizeof(wave_in_t));

        device = podule_callbacks->config_get_string(podule, "sound_in_device", "0");
        sscanf(device, "%i", &device_nr);
	if (log)
	        log("sound_in_init: device_nr=%i\n", device_nr);

        wave_in->p = p;
	wave_in->sound_in_buffer = sound_in_buffer;

        sprintf(portname, "hw:%i,%i,%i", in_devices[device_nr].card,
					 in_devices[device_nr].device,
					 in_devices[device_nr].sub);
        if (log)
                log("Opening input port %s\n", portname);

	if (snd_pcm_open(&wave_in->device, portname, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK) < 0)
	{
                if (log)
                        log("Failed to open input device\n");
        }
        else
        {        
                snd_pcm_hw_params_malloc(&hw_params);
                snd_pcm_hw_params_any(wave_in->device, hw_params);
                snd_pcm_hw_params_set_access(wave_in->device, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
                snd_pcm_hw_params_set_format(wave_in->device, hw_params, SND_PCM_FORMAT_S16_LE);
                snd_pcm_hw_params_set_rate_near(wave_in->device, hw_params, &rate, 0);
                snd_pcm_hw_params_set_channels(wave_in->device, hw_params, 2);
                snd_pcm_hw_params(wave_in->device, hw_params);
                snd_pcm_hw_params_free(hw_params);
                
                snd_pcm_prepare(wave_in->device);
                pthread_create(&wave_in->in_thread, NULL, wave_in_callback, wave_in);
        }

        return wave_in;
}

void sound_in_close(void *p)
{
        wave_in_t *wave_in = p;
        
        wave_in->in_thread_term = 1;
        pthread_join(wave_in->in_thread, NULL);

        if (wave_in->device != NULL)
        {
                snd_pcm_close(wave_in->device);
                wave_in->device = NULL;
        }

        free(wave_in);
}

#define IN_SLEEP_TIME 5000 /*5ms*/

void *wave_in_callback(void *p)
{
        wave_in_t *wave_in = p;
        
        while (!wave_in->in_thread_term)
        {
                if (wave_in->running)
                {
                        int status = snd_pcm_readi(wave_in->device, wave_in->buffer, 4800);
                
                        if (status > 0)
                                wave_in->sound_in_buffer(wave_in->p, wave_in->buffer, status);
                        else
                                usleep(IN_SLEEP_TIME);
                }
                else
                        usleep(IN_SLEEP_TIME);
        }
}

void sound_in_start(void *p)
{
        wave_in_t *wave_in = p;

        wave_in->running = 1;
}
void sound_in_stop(void *p)
{
        wave_in_t *wave_in = p;
        
        wave_in->running = 0;
}

podule_config_selection_t *sound_in_devices_config(void)
{
        int nr_devs;
        podule_config_selection_t *sel;
        podule_config_selection_t *sel_p;
        char *in_dev_text = malloc(65536);
        int c;
        
        if (!in_queried)
                in_query();

        nr_devs = in_device_count;
        sel = malloc(sizeof(podule_config_selection_t) * (nr_devs+2));
        sel_p = sel;

        strcpy(in_dev_text, "None");
        sel_p->description = in_dev_text;
        sel_p->value = -1;
        sel_p++;
        in_dev_text += strlen(in_dev_text)+1;

        for (c = 0; c < nr_devs; c++)
        {
                strcpy(in_dev_text, in_devices[c].name);
                sel_p->description = in_dev_text;
                sel_p->value = c;
                sel_p++;

                in_dev_text += strlen(in_dev_text)+1;
        }

        strcpy(in_dev_text, "");
        sel_p->description = in_dev_text;

        return sel;
}
