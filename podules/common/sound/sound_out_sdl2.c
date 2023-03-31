#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "sound_out.h"

typedef struct sdl_sound_t
{
	SDL_AudioDeviceID audio_device;

	int freq;
	int buffer_size;
} sdl_sound_t;

void sound_out_close(void *p)
{
	sdl_sound_t *sdl_sound = (sdl_sound_t *)p;

	SDL_CloseAudioDevice(sdl_sound->audio_device);

	free(sdl_sound);
}

void *sound_out_init(void *p, int freq, int buffer_size, void (*log)(const char *format, ...), const podule_callbacks_t *podule_callbacks, podule_t *podule)
{
	SDL_AudioSpec audio_spec = {0};
	sdl_sound_t *sdl_sound = malloc(sizeof(sdl_sound_t));
	memset(sdl_sound, 0, sizeof(sdl_sound_t));

	sdl_sound->freq = freq;
	sdl_sound->buffer_size = buffer_size;

	SDL_Init(SDL_INIT_AUDIO);

	audio_spec.freq = freq;
	audio_spec.format = AUDIO_S16SYS;
	audio_spec.channels = 2;
	audio_spec.samples = 1024;
	audio_spec.callback = NULL;

	sdl_sound->audio_device = SDL_OpenAudioDevice(NULL, 0, &audio_spec, NULL, 0);

	if (!sdl_sound->audio_device)
	{
		free(sdl_sound);
		return NULL;
	}

	SDL_PauseAudioDevice(sdl_sound->audio_device, 0);

	return sdl_sound;
}

void sound_out_buffer(void *p, int16_t *buf, int len)
{
	sdl_sound_t *sdl_sound = (sdl_sound_t *)p;

	/*If we're already sufficiently ahead of the audio device then drop this buffer rather than
	  allowing the queued audio to build up indefinitely*/
	if (SDL_GetQueuedAudioSize(sdl_sound->audio_device) > (sdl_sound->buffer_size * 4 * 4))
		return;

	SDL_QueueAudio(sdl_sound->audio_device, buf, len*4);
}
