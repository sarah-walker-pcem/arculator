#include <stdio.h>
#include <windows.h>
#include <mmsystem.h>
#include <stdint.h>
#include "podule_api.h"
#include "midi.h"

typedef struct midi_t
{
        int pos, len;
        uint32_t command;
        int insysex;
        uint8_t sysex_data[1024+2];
        
        HMIDIOUT out_device;
        HMIDIIN in_device;

        void (*receive)(void *p, uint8_t val);
        
        void *p;
} midi_t;

static int midi_id;

void midi_close();

void midi_get_dev_name(int num, char *s);
void midi_in_get_dev_name(int num, char *s);

static void CALLBACK midi_in_callback(HMIDIIN hMidiIn, UINT wMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2);

void *midi_init(void *p, void (*receive)(void *p, uint8_t val), void (*log)(const char *format, ...), const podule_callbacks_t *podule_callbacks, podule_t *podule)
{
        midi_t *midi;
        MMRESULT hr;
        char name[256];
        int c;
        const char *device;
        int midi_in_dev_nr, midi_out_dev_nr;
        
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
        
        midi_id = 0;//config_get_int(CFG_MACHINE, NULL, "midi", 0);
        if (log)
                log("num_out_devs=%i\n", midiOutGetNumDevs());
        for (c = 0; c < midiOutGetNumDevs(); c++)
        {
                midi_get_dev_name(c, name);
                if (log)
                        log("name%i = %s\n", c, name);
        }

        if (log)
                log("num_in_devs=%i\n", midiInGetNumDevs());
        for (c = 0; c < midiInGetNumDevs(); c++)
        {
                midi_in_get_dev_name(c, name);
                if (log)
                        log("name%i = %s\n", c, name);
        }

        hr = midiOutOpen(&midi->out_device, midi_id, 0,
		   0, CALLBACK_NULL);
        if (hr != MMSYSERR_NOERROR) {
//                lark_log("midiOutOpen error - %08X\n",hr);
                midi_id = 0;
                hr = midiOutOpen(&midi->out_device, midi_id, 0,
        		   0, CALLBACK_NULL);
                if (hr != MMSYSERR_NOERROR) {
//                        lark_log("midiOutOpen error - %08X\n",hr);
                        return midi;
                }
        }
        midiOutReset(midi->out_device);

        midi_id = 0;
        hr = midiInOpen(&midi->in_device, midi_id, (DWORD)(void *)midi_in_callback,
		   (DWORD)midi, CALLBACK_FUNCTION);
        if (hr != MMSYSERR_NOERROR) {
//                lark_log("midiInOpen error - %08X\n",hr);
                midi_id = 0;
                hr = midiInOpen(&midi->in_device, midi_id, (DWORD)(void *)midi_in_callback,
        		   (DWORD)midi, CALLBACK_FUNCTION);
                if (hr != MMSYSERR_NOERROR) {
//                        lark_log("midiInOpen error - %08X\n",hr);
                        return midi;
                }
        }
        midiInStart(midi->in_device);
        
        return midi;
}

void midi_close(void *p)
{
        midi_t *midi = p;
        
        if (midi->in_device != NULL)
        {
                midiInReset(midi->in_device);
                midiInClose(midi->in_device);
                midi->in_device = NULL;
        }
        if (midi->out_device != NULL)
        {
                midiOutReset(midi->out_device);
                midiOutClose(midi->out_device);
                midi->out_device = NULL;
        }
}

int midi_get_num_devs()
{
        return midiOutGetNumDevs();
}
void midi_get_dev_name(int num, char *s)
{
        MIDIOUTCAPS caps;

        midiOutGetDevCaps(num, &caps, sizeof(caps));
        strcpy(s, caps.szPname);
}
void midi_in_get_dev_name(int num, char *s)
{
        MIDIINCAPS caps;

        midiInGetDevCaps(num, &caps, sizeof(caps));
        strcpy(s, caps.szPname);
}

static int midi_lengths[8] = {3, 3, 3, 3, 2, 2, 3, 1};

static void CALLBACK midi_in_callback(HMIDIIN hMidiIn, UINT wMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
        midi_t *midi = (midi_t *)dwInstance;
        int length;
        MIDIHDR *hdr;
        int c;
        
//        lark_log("midi_in_callback %i\n", wMsg);
        
        switch (wMsg)
        {
                case MIM_DATA:
                length = midi_lengths[(dwParam1 & 0x70) >> 4];
/*                lark_log("midi_in_callback: %02x %02x %02x (%i)\n",
                        dwParam1 & 0xFF,
                        (dwParam1 >> 8) & 0xFF,
                        (dwParam1 >> 16) & 0xFF,
                        length);*/
                midi->receive(midi->p, dwParam1 & 0xFF);
                if (length >= 2)
                        midi->receive(midi->p, (dwParam1 >> 8) & 0xFF);
                if (length >= 3)
                        midi->receive(midi->p, (dwParam1 >> 16) & 0xFF);
                break;
                
                case MIM_LONGDATA:
                hdr = (MIDIHDR *)dwParam1;
                for (c = 0; c < hdr->dwBytesRecorded; c++)
                        midi->receive(midi->p, hdr->lpData[c]);
                break;
        }
}

static void midi_send_sysex(midi_t *midi)
{
        MIDIHDR hdr;
        
        hdr.lpData = (LPSTR)midi->sysex_data;
        hdr.dwBufferLength = midi->pos;
        hdr.dwFlags = 0;
        
/*        pclog("Sending sysex : ");
        for (c = 0; c < midi_pos; c++)
                pclog("%02x ", midi_sysex_data[c]);
        pclog("\n");*/
        
        midiOutPrepareHeader(midi->out_device, &hdr, sizeof(MIDIHDR));
        midiOutLongMsg(midi->out_device, &hdr, sizeof(MIDIHDR));
        
        midi->insysex = 0;
}

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
                        midi_send_sysex(midi);
                return;
        }
                        
        if (midi->len)
        {                
                midi->command |= (val << (midi->pos * 8));
                
                midi->pos++;
                
                if (midi->pos == midi->len)
                        midiOutShortMsg(midi->out_device, midi->command);
        }
}

podule_config_selection_t *midi_out_devices_config(void)
{
        int nr_devs = midiOutGetNumDevs();
        podule_config_selection_t *sel = malloc(sizeof(podule_config_selection_t) * (nr_devs+2));
        podule_config_selection_t *sel_p = sel;
        char *midi_dev_text = malloc(65536);
        int c;

        strcpy(midi_dev_text, "None");
        sel_p->description = midi_dev_text;
        sel_p->value = -1;
        sel_p++;
        midi_dev_text += strlen(midi_dev_text)+1;

        for (c = 0; c < nr_devs; c++)
        {
                midi_get_dev_name(c, midi_dev_text);
                sel_p->description = midi_dev_text;
                sel_p->value = c;
                sel_p++;

                midi_dev_text += strlen(midi_dev_text)+1;
        }

        strcpy(midi_dev_text, "");
        sel_p->description = midi_dev_text;

        return sel;
}
podule_config_selection_t *midi_in_devices_config(void)
{
        int nr_devs = midiInGetNumDevs();
        podule_config_selection_t *sel = malloc(sizeof(podule_config_selection_t) * (nr_devs+2));
        podule_config_selection_t *sel_p = sel;
        char *midi_dev_text = malloc(65536);
        int c;
        
        strcpy(midi_dev_text, "None");
        sel_p->description = midi_dev_text;
        sel_p->value = -1;
        sel_p++;
        midi_dev_text += strlen(midi_dev_text)+1;
                
        for (c = 0; c < nr_devs; c++)
        {
                midi_in_get_dev_name(c, midi_dev_text);
                sel_p->description = midi_dev_text;
                sel_p->value = c;
                sel_p++;

                midi_dev_text += strlen(midi_dev_text)+1;
        }

        strcpy(midi_dev_text, "");
        sel_p->description = midi_dev_text;

        return sel;
}
