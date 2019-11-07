/*Arculator 2.0 by Sarah Walker
  Disc drive noise*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arc.h"
#include "config.h"
#include "disc.h"
#include "ddnoise.h"
#include "soundopenal.h"
#include "timer.h"

int ddnoise_vol=3;
int ddnoise_type=0;

typedef struct SAMPLE
{
        int16_t *data;
        int len;
        int freq;
} SAMPLE;

typedef struct chunk_t
{
        char id[4];
        uint32_t size;
} chunk_t;

typedef struct fmt_t
{
        uint16_t wFormatTag;
        uint16_t wChannels;
        uint32_t dwSamplesPerSec;
        uint32_t dwAvgBytesPerSec;
        uint16_t wBlockAlign;
        uint16_t wFmtSpecific;
} fmt_t;

static SAMPLE *load_wav(char *path, const char *fn)
{
        char path_fn[512];
        SAMPLE *s;
        chunk_t chunk;
        fmt_t fmt;
        int got_fmt = 0;
        uint8_t wave_id[4];
        FILE *f;
        
        append_filename(path_fn, path, fn, sizeof(path_fn));
        f = fopen(path_fn, "rb");
        if (!f)
        {
                rpclog("Can't open %s\n", path_fn);
                return NULL;
        }

        /*Read initial chunk*/
        fread(&chunk, sizeof(chunk_t), 1, f);
        if (chunk.id[0] != 'R' || chunk.id[1] != 'I' || chunk.id[2] != 'F' || chunk.id[3] != 'F')
        {
                rpclog("%s is not a RIFF file %c %c %c %c\n", path_fn,
                                chunk.id[0], chunk.id[1], chunk.id[2], chunk.id[3]);
                fclose(f);
                return NULL;
        }

        fread(wave_id, sizeof(wave_id), 1, f);
        if (wave_id[0] != 'W' || wave_id[1] != 'A' || wave_id[2] != 'V' || wave_id[3] != 'E')
        {
                rpclog("%s is not a RIFF WAVE file %c %c %c %c\n", path_fn,
                                wave_id[0], wave_id[1], wave_id[2], wave_id[3]);
                fclose(f);
                return NULL;
        }

        /*Search for WAVE chunk*/
        while (!feof(f))
        {
                fread(&chunk, sizeof(chunk_t), 1, f);
                if (chunk.id[0] == 'f' && chunk.id[1] == 'm' && chunk.id[2] == 't' && chunk.id[3] == ' ')
                {
//                        rpclog("Found fmt chunk\n");
                        fread(&fmt, sizeof(fmt_t), 1, f);
                        if (fmt.wFormatTag != 1) /*Only support PCM*/
                        {
                                rpclog("%s is not a PCM WAVE file\n", path_fn);
                                fclose(f);
                                return NULL;
                        }
//                        rpclog("wChannels=%i\n", fmt.wChannels);
//                        rpclog("dwSamplesPerSec=%i\n", fmt.dwSamplesPerSec);
//                        rpclog("wFmtSpecific=%i\n", fmt.wFmtSpecific);
                        got_fmt = 1;
                }
                else if (chunk.id[0] == 'd' && chunk.id[1] == 'a' && chunk.id[2] == 't' && chunk.id[3] == 'a')
                {
                        rpclog("Found data chunk, len=%i\n", chunk.size);
                        if (!got_fmt)
                        {
                                rpclog("%s has data before fmt\n", path_fn);
                                fclose(f);
                                return NULL;
                        }
                        s = malloc(sizeof(SAMPLE));
                        s->freq = fmt.dwSamplesPerSec;
                        if (fmt.wFmtSpecific == 16)
                        {
                                if (fmt.wChannels == 2)
                                {
                                        /*Stereo, downmix to mono*/
                                        int c;

//                                        rpclog("16-bit, stereo\n");
                                        s->len = chunk.size / 4;
                                        s->data = malloc(s->len*2);
                                        for (c = 0; c < s->len; c++)
                                        {
                                                int16_t samples[2];
                                                
                                                fread(samples, sizeof(samples), 1, f);
                                                s->data[c] = (samples[0] + samples[1]) / 2;
                                        }
                                }
                                else
                                {
//                                        rpclog("16-bit, mono\n");
                                        s->len = chunk.size / 2;
                                        s->data = malloc(s->len*2);
                                        fread(s->data, chunk.size, 1, f);
                                }
                        }
                        else /*8 bits per sample, need to upconvert*/
                        {
                                if (fmt.wChannels == 2)
                                {
                                        /*Stereo, downmix to mono*/
                                        int c;

//                                        rpclog("8-bit, stereo\n");
                                        s->len = chunk.size / 2;
                                        s->data = malloc(s->len*2);
                                        for (c = 0; c < s->len; c++)
                                        {
                                                uint8_t samples[2];

                                                fread(samples, sizeof(samples), 1, f);
                                                s->data[c] = ((int16_t)(samples[0] ^ 0x80) + (int16_t)(samples[1] ^ 0x80)) * 128;
                                        }
                                }
                                else
                                {
                                        int c;
                                        
//                                        rpclog("8-bit, mono\n");
                                        s->len = chunk.size;
                                        s->data = malloc(s->len*2);
                                        for (c = 0; c < chunk.size; c++)
                                                s->data[c] = (getc(f) ^ 0x80) << 8;
                                }
                        }
                        fclose(f);
                        return s;
                }
                else
                {
                        /*Skip over chunk*/
                        fseek(f, chunk.size, SEEK_CUR);
                        if (chunk.size & 1)
                                fseek(f, 1, SEEK_CUR);
                }
        }
        
        rpclog("%s does not have WAVE chunk\n", path_fn);
        fclose(f);
        
        return NULL;
//        s = malloc(sizeof(SAMPLE));
        
}

static void destroy_sample(SAMPLE *s)
{
        if (s)
        {
                if (s->data)
                        free(s->data);
                free(s);
        }
}

static SAMPLE *seeksmp[4][2];
static SAMPLE *motorsmp[3];

static float ddnoise_mpos = 0;
static int ddnoise_mstat = -1;
static int oldmotoron = 0;

static float ddnoise_spos = 0;
static int ddnoise_sstat = -1;
static int ddnoise_sdir = 0;

void ddnoise_init()
{
        char path[512];

        append_filename(path, exname, "ddnoise/35/", sizeof(path));
        rpclog("ddnoise path %s\n", path);

        seeksmp[0][0] = load_wav(path, "stepo.wav");
        if (seeksmp[0][0])
        {
                seeksmp[0][1] = load_wav(path, "stepi.wav");
                seeksmp[1][0] = load_wav(path, "seek1o.wav");
                seeksmp[1][1] = load_wav(path, "seek1i.wav");
                seeksmp[2][0] = load_wav(path, "seek2o.wav");
                seeksmp[2][1] = load_wav(path, "seek2i.wav");
                seeksmp[3][0] = load_wav(path, "seek3o.wav");
                seeksmp[3][1] = load_wav(path, "seek3i.wav");
        }
        else
        {
                seeksmp[0][0] = load_wav(path, "step.wav");
                seeksmp[0][1] = load_wav(path, "step.wav");
                seeksmp[1][0] = load_wav(path, "seek.wav");
                seeksmp[1][1] = load_wav(path, "seek.wav");
                seeksmp[2][0] = load_wav(path, "seek3.wav");
                seeksmp[2][1] = load_wav(path, "seek3.wav");
                seeksmp[3][0] = load_wav(path, "seek2.wav");
                seeksmp[3][1] = load_wav(path, "seek2.wav");
        }
        motorsmp[0] = load_wav(path, "motoron.wav");
        motorsmp[1] = load_wav(path, "motor.wav");
        motorsmp[2] = load_wav(path, "motoroff.wav");
}

void ddnoise_close()
{
        int c;
        for (c = 0; c < 4; c++)
        {
                if (seeksmp[c][0]) destroy_sample(seeksmp[c][0]);
                if (seeksmp[c][1]) destroy_sample(seeksmp[c][1]);
                seeksmp[c][0] = seeksmp[c][1] = NULL;
        }
        for (c = 0; c < 3; c++)
        {
                if (motorsmp[c]) destroy_sample(motorsmp[c]);
                motorsmp[c] = NULL;
        }
}

static int16_t ddbuffer[4410];

void ddnoise_seek(int len)
{
//        timer_set_delay_u64(&fdc_timer, 2500 * TIMER_USEC);
//        rpclog("Seek %i tracks\n",len);
        ddnoise_sdir = (len < 0) ? 1 : 0;
        if (len < 0) len = -len;
        ddnoise_spos = 0;
        if (len == 0)
        {
                ddnoise_sstat = -1;
                timer_set_delay_u64(&fdc_timer, 200 * TIMER_USEC);
//                rpclog("fdc_time set by ddnoise_seek 0\n");
        }
        else if (len == 1)
        {
                ddnoise_sstat = 0;
                timer_set_delay_u64(&fdc_timer, 6000 * TIMER_USEC);
//                rpclog("fdc_time set by ddnoise_seek 1\n");
        }
        else if (len < 7)
                ddnoise_sstat = 1;
        else if (len < 30)
                ddnoise_sstat = 2;
        else
                ddnoise_sstat = 3;
//        if (!sound_ddnoise) fdc_time = 200;
//        rpclog("Start seek!\n");
}

void ddnoise_mix()
{
        int c;
//        if (!f1) f1=fopen("f1.pcm","wb");
//        if (!f2) f2=fopen("f2.pcm","wb");

        memset(ddbuffer, 0, 4410 * 2);
//        fwrite(ddbuffer,4410*2,1,f1);
        if (motoron && !oldmotoron)
        {
                ddnoise_mstat = 0;
                ddnoise_mpos = 0;
        }
        if (!motoron && oldmotoron)
        {
                ddnoise_mstat = 2;
                ddnoise_mpos = 0;
        }
        
        if (1)//sound_ddnoise)
        {
                for (c = 0; c < 4410; c++)
                {
                        ddbuffer[c] = 0;
                        if (ddnoise_mstat >= 0)
                        {
                                if (ddnoise_mpos >= motorsmp[ddnoise_mstat]->len)
                                {
                                        ddnoise_mpos = 0;
                                        if (ddnoise_mstat != 1) ddnoise_mstat++;
                                        if (ddnoise_mstat == 3) ddnoise_mstat = -1;
                                }
                                if (ddnoise_mstat != -1)
                                {
                                        ddbuffer[c] += ((int16_t)(((int16_t *)motorsmp[ddnoise_mstat]->data)[(int)ddnoise_mpos]) / 2);
                                        ddnoise_mpos += ((float)motorsmp[ddnoise_mstat]->freq / 44100.0);
                                }
                        }
                }

                for (c = 0; c < 4410; c++)
                {
                        if (ddnoise_sstat >= 0)
                        {
                                if (ddnoise_spos >= seeksmp[ddnoise_sstat][ddnoise_sdir]->len)
                                {
                                        if (ddnoise_sstat > 0)
                                        {
//                                                rpclog("fdc_time set by ddnoise_mix\n");
                                                timer_set_delay_u64(&fdc_timer, 200 * TIMER_USEC);
                                        }
                                        ddnoise_spos = 0;
                                        ddnoise_sstat = -1;
                                }
                                else
                                {
                                        ddbuffer[c] += ((int16_t)(((int16_t *)seeksmp[ddnoise_sstat][ddnoise_sdir]->data)[(int)ddnoise_spos]) / 2);
                                        ddnoise_spos += ((float)seeksmp[ddnoise_sstat][ddnoise_sdir]->freq / 44100.0);
                                }
                        }
                        ddbuffer[c] = (ddbuffer[c] / 3) * ddnoise_vol;
                }
        }
        
        al_givebufferdd(ddbuffer);
        
        oldmotoron=motoron;
}
