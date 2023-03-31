#include <SDL2/SDL.h>
#include "plat_sound.h"
#include "disc.h"
#include "sound.h"

#define DDNOISE_FREQ 44100
#define OUTPUT_FREQ 48000

static SDL_AudioDeviceID audio_device;
static SDL_AudioStream *ddnoise_stream;

void sound_dev_init(void)
{
	SDL_AudioSpec audio_spec = {0};

	audio_spec.freq = OUTPUT_FREQ;
	audio_spec.format = AUDIO_S16SYS;
	audio_spec.channels = 2;
	audio_spec.samples = 1024;
	audio_spec.callback = NULL;

	audio_device = SDL_OpenAudioDevice(NULL, 0, &audio_spec, NULL, 0);
	rpclog("audio_device=%u\n", audio_device);

	ddnoise_stream = SDL_NewAudioStream(AUDIO_S16SYS, 1, DDNOISE_FREQ, AUDIO_S16SYS, 2, OUTPUT_FREQ);

	SDL_PauseAudioDevice(audio_device, 0);
}

void sound_dev_close(void)
{
	SDL_FreeAudioStream(ddnoise_stream);
	SDL_CloseAudioDevice(audio_device);
}

#define MAX_QUEUED_SIZE ((OUTPUT_FREQ * 4) / 5) /*200ms*/
#define MAX_DDNOISE_STREAM_SIZE ((DDNOISE_FREQ * 4) / 5) /*200ms*/

void sound_givebuffer(int16_t *buf)
{
	int ddnoise_gain = (int)(pow(10.0, (double)disc_noise_gain / 20.0) * 256.0);
	int gain = (int)(pow(10.0, (double)sound_gain / 20.0) * 256.0);
	int16_t ddnoise_buffer[2400*2];
	int len;

	/*If we're already sufficiently ahead of the audio device then drop this buffer rather than
	  allowing the queued audio to build up indefinitely*/
	if (SDL_GetQueuedAudioSize(audio_device) > MAX_QUEUED_SIZE)
		return;

	for (int i = 0; i < 2400*2; i++)
	{
		int32_t sample = (buf[i] * gain) >> 8;

		buf[i] = (sample < -32768) ? -32768 : ((sample > 32767) ? 32767 : sample);
	}

	len = SDL_AudioStreamGet(ddnoise_stream, ddnoise_buffer, sizeof(ddnoise_buffer));

	for (int i = 0; i < (len / 2); i++)
	{
		int32_t sample = buf[i] + ((ddnoise_buffer[i] * ddnoise_gain) >> 8);

		buf[i] = (sample < -32768) ? -32768 : ((sample > 32767) ? 32767 : sample);
	}

	SDL_QueueAudio(audio_device, buf, 2400*4);
}

void sound_givebufferdd(int16_t *buf)
{
	/*If we're already sufficiently ahead of the audio device then drop this buffer rather than
	  allowing the queued audio to build up indefinitely*/
	if (SDL_AudioStreamAvailable(ddnoise_stream) > MAX_DDNOISE_STREAM_SIZE)
		return;

	SDL_AudioStreamPut(ddnoise_stream, buf, 4410*2);
}
