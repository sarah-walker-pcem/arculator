#include <windows.h>
#include <mmsystem.h>
#include <stdint.h>
#include <stdio.h>
#include "podule_api.h"
#include "sound_in.h"

typedef struct wave_in_t
{
        HWAVEIN device;
        
        WAVEHDR buffer_hdrs[2];
        int16_t buffers[2][4800*2];
        
        int running;
        int buffers_pending;

	void (*sound_in_buffer)(void *p, void *buffer, int samples);
        void *p;
} wave_in_t;

static void CALLBACK wave_in_callback(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2);

void *sound_in_init(void *p, void (*sound_in_buffer)(void *p, void *buffer, int samples), void (*log)(const char *format, ...), const podule_callbacks_t *podule_callbacks, podule_t *podule)
{
        int num_devs = waveInGetNumDevs();
        int c;
        WAVEFORMATEX format;
        MMRESULT hr;
        const char *device;
        int device_nr = 0;
        wave_in_t *wave_in = malloc(sizeof(wave_in_t));
        memset(wave_in, 0, sizeof(wave_in_t));

        device = podule_callbacks->config_get_string(podule, "sound_in_device", "0");
        sscanf(device, "%i", &device_nr);
	if (log)
	        log("sound_in_init: device_nr=%i\n", device_nr);

        wave_in->p = p;
	wave_in->sound_in_buffer = sound_in_buffer;

	if (log)
	        log("%i WaveIn devs %p\n", num_devs, wave_in);
        for (c = 0; c < num_devs; c++)
        {
                WAVEINCAPS caps;
                
                waveInGetDevCaps(c, &caps, sizeof(caps));
		if (log)
                	log("dev %i : %s, formats=%08x\n", c, caps.szPname, caps.dwFormats);
        }
        
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = 2;
        format.nSamplesPerSec = 48000;
        format.nAvgBytesPerSec = 48000*2*2;
        format.nBlockAlign = 4;
        format.wBitsPerSample = 16;
        format.cbSize = 0;
        
        hr = waveInOpen(&wave_in->device, device_nr, &format, (DWORD_PTR)wave_in_callback,
                        (DWORD_PTR)wave_in, CALLBACK_FUNCTION);
        if (hr != MMSYSERR_NOERROR)
        {
                char string[256];
		if (log)
	                log("waveInOpen failed %i\n", hr);
                waveInGetErrorText(hr, string, 255);
		if (log)
	                log("%s\n", string);
        }
        
        for (c = 0; c < 2; c++)
        {
                WAVEHDR *hdr = &wave_in->buffer_hdrs[c];
                
                memset(hdr, 0, sizeof(WAVEHDR));
                hdr->lpData = (LPSTR)wave_in->buffers[c];
                hdr->dwBufferLength = 4800*2*2;
                hdr->dwBytesRecorded = 0;
                hdr->dwUser = (DWORD_PTR)wave_in;
                hdr->dwFlags = 0;
                
                hr = waveInPrepareHeader(wave_in->device, hdr, sizeof(WAVEHDR));
                if (hr != MMSYSERR_NOERROR)
		{
			if (log)
	                        log("waveInPrepareHeader %i failed %i\n", c, hr);
		}
        }
                
        return wave_in;
}

void sound_in_close(void *p)
{
        wave_in_t *wave_in = p;
        
        waveInUnprepareHeader(wave_in->device, &wave_in->buffer_hdrs[0], sizeof(WAVEHDR));
        waveInUnprepareHeader(wave_in->device, &wave_in->buffer_hdrs[1], sizeof(WAVEHDR));
        waveInReset(wave_in->device);
        waveInClose(wave_in->device);
        free(wave_in);
}

static void CALLBACK wave_in_callback(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
        wave_in_t *wave_in = (wave_in_t *)dwInstance;
        WAVEHDR *hdr;
        MMRESULT hr;
        
        switch (uMsg)
        {
                case WIM_DATA:
                hdr = (WAVEHDR *)dwParam1;
                if (wave_in->running)
                {
                        wave_in->sound_in_buffer(wave_in->p, hdr->lpData, hdr->dwBytesRecorded / 4);
                        waveInAddBuffer(wave_in->device, hdr, sizeof(WAVEHDR));
                }
                else if (wave_in->buffers_pending)
                        wave_in->buffers_pending--;
                break;
        }
}

void sound_in_start(void *p)
{
        wave_in_t *wave_in = p;
        int c;
        int sleeps = 0;
        
        while (wave_in->buffers_pending)
        {
                Sleep(100);
                sleeps++;
                if (sleeps >= 5)
                {
//                        lark_log("Timed out on buffers pending %i\n", wave_in->buffers_pending);
                        break;
                }
        }
        
        for (c = 0; c < 2; c++)
                waveInAddBuffer(wave_in->device, &wave_in->buffer_hdrs[c], sizeof(WAVEHDR));

        wave_in->running = 1;
        waveInStart(wave_in->device);
}
void sound_in_stop(void *p)
{
        wave_in_t *wave_in = p;
        
//        lark_log("sound_in_stop\n");
        wave_in->running = 0;
        wave_in->buffers_pending = 2;
        waveInReset(wave_in->device);
}

podule_config_selection_t *sound_in_devices_config(void)
{
        int nr_devs = waveInGetNumDevs();
        podule_config_selection_t *sel = malloc(sizeof(podule_config_selection_t) * (nr_devs+2));
        podule_config_selection_t *sel_p = sel;
        char *wave_dev_text = malloc(65536);
        int c;

        strcpy(wave_dev_text, "None");
        sel_p->description = wave_dev_text;
        sel_p->value = -1;
        sel_p++;
        wave_dev_text += strlen(wave_dev_text)+1;

        for (c = 0; c < nr_devs; c++)
        {
                WAVEINCAPS caps;

                waveInGetDevCaps(c, &caps, sizeof(caps));
                strcpy(wave_dev_text, caps.szPname);

                sel_p->description = wave_dev_text;
                sel_p->value = c;
                sel_p++;

                wave_dev_text += strlen(wave_dev_text)+1;
        }

        strcpy(wave_dev_text, "");
        sel_p->description = wave_dev_text;

        return sel;
}
