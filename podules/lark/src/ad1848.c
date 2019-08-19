#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "podule_api.h"
#include "ad1848.h"
#include "lark.h"

static int wss_vols[64];

static void generate_fir(ad1848_t *ad1848, int sample_rate, int cutoff);
static void ad1848_process_in_buffer(ad1848_t *ad1848, int16_t *buffer, int samples);

/*Output uses a low-pass filter with cutoff of Fs * 0.45*/
static int16_t output_iir(ad1848_t *ad1848, int i, int16_t NewSample)
{
        static double y[2][3]; //output samples
        static double x[2][3]; //input samples

        //shift the old samples
        x[i][2] = x[i][1];
        y[i][2] = y[i][1];
        x[i][1] = x[i][0];
        y[i][1] = y[i][0];

        //Calculate the new output
        x[i][0] = (double)NewSample;
        y[i][0] = ad1848->output_iir_coef_a[0] * x[i][0];
        y[i][0] += ad1848->output_iir_coef_a[1] * x[i][1] - ad1848->output_iir_coef_b[1] * y[i][1];
        y[i][0] += ad1848->output_iir_coef_a[2] * x[i][2] - ad1848->output_iir_coef_b[2] * y[i][2];

        return (int16_t)y[i][0];
}

static void output_iir_gen_coefficients(const int samplerate, const double cutoff, double* const ax, double* const by)
{
        double ff = cutoff/samplerate;
        const double ita = 1.0 / tan(M_PI*ff);
        const double q = sqrt(2.0);

        ax[0] = 1.0 / (1.0 + q*ita + ita*ita);
        ax[1]= 2*ax[0];
        ax[2]= ax[0];
        by[0] = 1.0;
        by[1] = -(2.0 * (ita*ita - 1.0) * ax[0]);
        by[2] = (1.0 - q*ita + ita*ita) * ax[0];
}

/*Input uses a brickwall low-pass FIR filter with cutoff of Fs * 0.5*/
static inline float input_fir(ad1848_t *ad1848, int i, float NewSample)
{
        static float x[2][128]; //input samples
        static int pos = 0;
        float out = 0.0;
	int read_pos;
	int n_coef;

        x[i][pos] = NewSample;

        /*Since only 1/16th of input samples are non-zero, only filter those that
          are valid.*/
	read_pos = (pos + 15) & (127 & ~15);
	n_coef = (16 - pos) & 15;

	while (n_coef < AD1848_NCoef)
	{
		out += ad1848->low_fir_coef[n_coef] * x[i][read_pos];
		read_pos = (read_pos + 16) & (127 & ~15);
		n_coef += 16;
	}

        if (i == 1)
        {
        	pos = (pos + 1) & 127;
        	if (pos > 127)
        		pos = 0;
        }

        return out;
}

static inline double sinc(double x)
{
	return sin(M_PI * x) / (M_PI * x);
}

static void input_fir_gen_coefficients(ad1848_t *ad1848, int sample_rate, int cutoff)
{
        /*Cutoff frequency = 1 / 32*/
        float fC = (float)cutoff / (float)sample_rate; //1.0 / 32.0;
        float gain;
        int n;

        lark_log("generate_fir: sample_rate=%i cutoff=%i fC=%f\n", sample_rate, cutoff, fC);
        for (n = 0; n < AD1848_NCoef; n++)
        {
                /*Blackman window*/
                double w = 0.42 - (0.5 * cos((2.0*n*M_PI)/(double)(AD1848_NCoef-1))) + (0.08 * cos((4.0*n*M_PI)/(double)(AD1848_NCoef-1)));
                /*Sinc filter*/
                double h = sinc(2.0 * fC * ((double)n - ((double)(AD1848_NCoef-1) / 2.0)));

                /*Create windowed-sinc filter*/
                ad1848->low_fir_coef[n] = w * h;
        }

        ad1848->low_fir_coef[(AD1848_NCoef - 1) / 2] = 1.0;

        gain = 0.0;
        for (n = 0; n < AD1848_NCoef; n++)
                gain += ad1848->low_fir_coef[n] / (float)16;

        gain /= 0.95;

        /*Normalise filter, to produce unity gain*/
        for (n = 0; n < AD1848_NCoef; n++)
                ad1848->low_fir_coef[n] /= gain;
}


uint8_t ad1848_read(ad1848_t *ad1848, uint16_t addr)
{
        uint8_t temp = 0xff;
//        lark_log("wss_read - addr %04X ", addr);
        switch (addr & 7)
        {
                case 0: case 1: case 2: case 3: /*Version*/
                temp = 4 | (ad1848->config & 0x40);
                break;
                
                case 4: /*Index*/
                temp = ad1848->index | ad1848->trd | ad1848->mce;
                break;
                case 5:
                temp = ad1848->regs[ad1848->index];
                break;
                case 6:
                temp = ad1848->status;
                break;
        }
//        lark_log("return %02X\n", temp);
        return temp;
}

void ad1848_write(ad1848_t *ad1848, uint16_t addr, uint8_t val)
{
        double freq, base_freq;
//        lark_log("wss_write - addr %04X val %02X\n", addr, val);
        switch (addr & 7)
        {
                case 0: case 1: case 2: case 3: /*Config*/
                ad1848->config = val;
                break;
                
                case 4: /*Index*/
                ad1848->index = val & 0xf;
                ad1848->trd   = val & 0x20;
                ad1848->mce   = val & 0x40;
                break;
                case 5:
                switch (ad1848->index)
                {
                        case 8:
                        base_freq = (val & 1) ? 16934400 : 24576000;
                        switch ((val >> 1) & 7)
                        {
                                case 0: freq = base_freq / 3072; ad1848->rate_divider = 48; break;
                                case 1: freq = base_freq / 1536; ad1848->rate_divider = 24; break;
                                case 2: freq = base_freq / 896;  ad1848->rate_divider = 14; break;
                                case 3: freq = base_freq / 768;  ad1848->rate_divider = 12; break;
                                case 4: freq = base_freq / 448;  ad1848->rate_divider = 7;  break;
                                case 5: freq = base_freq / 384;  ad1848->rate_divider = 6;  break;
                                case 6: freq = base_freq / 512;  ad1848->rate_divider = 8;  break;
                                case 7: freq = base_freq / 2560; ad1848->rate_divider = 40; break;
                        }
                        lark_log("wss freq now %f Hz\n", freq);
                        input_fir_gen_coefficients(ad1848, 48000*16, freq/2);
                        ad1848->in_i_inc = (int)((0x80000ull * 48000) / freq);
//                        lark_log("i_inc=%08x\n", ad1848->in_i_inc);
                        
                        ad1848->sample_freq = freq;

                        ad1848->sample_time_us = 1000000.0 / freq;
                        ad1848->samp_inc = ((int)(((double)(base_freq / 64) / 48000.0) * 16384.0));
                        
                        if ((val & 0xf) != (ad1848->regs[8] & 0xf))
                                output_iir_gen_coefficients(base_freq / 64, (double)freq * 0.45, ad1848->output_iir_coef_a, ad1848->output_iir_coef_b);
                        break;
                        
                        case 9:
                        ad1848->playback_enable = ((val & 0x41) == 0x01);
                        if (((val & 0x82) == 0x02) && !ad1848->capture_enable)
                        {
                                ad1848->in_rp = 0;
                                ad1848->in_wp = 0;
                                ad1848->in_ip = 0;
                                ad1848->in_f_wp = 0;
                                lark_sound_in_start(ad1848->lark);
                        }
                        if (((val & 0x82) != 0x02) && ad1848->capture_enable)
                                lark_sound_in_stop(ad1848->lark);
                        ad1848->capture_enable = ((val & 0x82) == 0x02);
                        break;
                                
                        case 12:
                        return;
                        
                        case 14:
                        ad1848->count = ad1848->regs[15] | (val << 8);
//                        lark_log("wss count now %04X\n", ad1848->count);
                        break;
                }
                ad1848->regs[ad1848->index] = val;
                break;
                case 6:
                ad1848->status &= 0xfe;
                lark_clear_irq(ad1848->lark, IRQ_AD1848);
                break;              
        }
}

static void filter_and_downsample(ad1848_t *ad1848)
{
        int sound_write_ptr;
        
        if (ad1848->first_poll)
                return;

//        rpclog("mixsound: samp_fp=%i samp_wp=%i samp_rp=%i %08x %08x\n", samp_fp, samp_wp, (samp_rp >> 15) * 2, samp_rp, SAMP_INC);
        /*Apply output low-pass filter*/
        while (ad1848->fp != ad1848->wp)
        {
                ad1848->sound_in_buffer[ad1848->fp] = output_iir(ad1848, 0, ad1848->sound_in_buffer[ad1848->fp]);
                ad1848->fp++;
                ad1848->sound_in_buffer[ad1848->fp] = output_iir(ad1848, 1, ad1848->sound_in_buffer[ad1848->fp]);
                ad1848->fp++;
                ad1848->fp &= 0x7fffe;
        }

        /*Downsample to 48 kHz*/
        for (sound_write_ptr = 0; sound_write_ptr < 4800; sound_write_ptr++)
        {
                ad1848->sound_out_buffer[sound_write_ptr*2]     = ad1848->sound_in_buffer[(ad1848->rp >> 14) * 2];
                ad1848->sound_out_buffer[sound_write_ptr*2 + 1] = ad1848->sound_in_buffer[(ad1848->rp >> 14) * 2 + 1];
                ad1848->rp += ad1848->samp_inc;
        }

}

void ad1848_run(ad1848_t *ad1848, int timeslice_us)
{
        if (ad1848->in_buffer_pending_samples)
        {
                ad1848_process_in_buffer(ad1848, ad1848->in_buffer_raw, ad1848->in_buffer_pending_samples);
                ad1848->in_buffer_pending_samples = 0;
        }
        
        if (ad1848->playback_enable || ad1848->capture_enable)
        {
                int vol_l = (ad1848->regs[6] & 0x80) ? 0 : wss_vols[ad1848->regs[6] & 0x3f];
                int vol_r = (ad1848->regs[7] & 0x80) ? 0 : wss_vols[ad1848->regs[7] & 0x3f];
                
                ad1848->timing_error_us += timeslice_us;
                
                /*lark_log("ad1848_run: timing_error_us=%g sample_time_us=%g timeslice_us=%i\n",
                        ad1848->timing_error_us,
                        ad1848->sample_time_us,
                        timeslice_us);*/

                while (ad1848->timing_error_us > 0)
                {
                        if (ad1848->playback_enable)
                        {
                                int16_t samp_l, samp_r;
                                int c;
                        
                                //lark_log("ad1848_run: read sample\n");
                                samp_l = ad1848->dma_read(ad1848->lark);
                                samp_l |= ad1848->dma_read(ad1848->lark) << 8;
                                samp_r = ad1848->dma_read(ad1848->lark);
                                samp_r |= ad1848->dma_read(ad1848->lark) << 8;
                        
                                samp_l = (samp_l * vol_l) >> 16;
                                samp_r = (samp_r * vol_r) >> 16;
                        
                                for (c = 0; c < ad1848->rate_divider; c++)
                                {
                                        ad1848->sound_in_buffer[ad1848->wp++] = samp_l;
                                        ad1848->sound_in_buffer[ad1848->wp++] = samp_r;
                                        ad1848->wp &= 0x7fffe;
                                }
                        }
                        if (ad1848->capture_enable)
                        {
                                int16_t samp_l, samp_r, samp;

                                if (ad1848->in_rp != ad1848->in_wp)
                                {
                                        samp_l = ad1848->in_buffer[ad1848->in_rp & 0x7fff];
                                        ad1848->in_rp++;
                                        samp_r = ad1848->in_buffer[ad1848->in_rp & 0x7fff];
                                        ad1848->in_rp++;
                                }
                                else
                                        samp_l = samp_r = 0;
                                
                                switch (ad1848->regs[8] & 0x50)
                                {
                                        case 0x00:
                                        samp = (samp_l + samp_r) / 2;
                                        ad1848->dma_write(ad1848->lark, (samp_l >> 8) ^ 0x80);
                                        ad1848->dma_write(ad1848->lark, (samp_r >> 8) ^ 0x80);
                                        break;
                                        case 0x10:
                                        ad1848->dma_write(ad1848->lark, (samp_l >> 8) ^ 0x80);
                                        ad1848->dma_write(ad1848->lark, (samp_r >> 8) ^ 0x80);
                                        break;
                                        case 0x40:
                                        samp = (samp_l + samp_r) / 2;
                                        ad1848->dma_write(ad1848->lark, samp & 0xff);
                                        ad1848->dma_write(ad1848->lark, samp >> 8);
                                        break;
                                        case 0x50:
                                        ad1848->dma_write(ad1848->lark, samp_l & 0xff);
                                        ad1848->dma_write(ad1848->lark, samp_l >> 8);
                                        ad1848->dma_write(ad1848->lark, samp_r & 0xff);
                                        ad1848->dma_write(ad1848->lark, samp_r >> 8);
                                        break;
                                }
                        }
                        
                        ad1848->timing_error_us -= ad1848->sample_time_us;
                }
                
                if (ad1848->playback_enable)
                {
                        ad1848->sound_time -= timeslice_us;
                        if (ad1848->sound_time < 0)
                        {
                                ad1848->sound_time += 100*1000; /*100ms*/

                                filter_and_downsample(ad1848);
                                if (ad1848->first_poll)
                                        ad1848->first_poll = 0;

                                lark_sound_out_buffer(ad1848->lark, ad1848->sound_out_buffer, 4800);
                        }
                }
        }
        else
                ad1848->timing_error_us = 0;
}

void ad1848_init(ad1848_t *ad1848, struct lark_t *lark, uint8_t (*dma_read)(struct lark_t *lark), void (*dma_write)(struct lark_t *lark, uint8_t val))
{
        int c;
        double attenuation;
        
//        lark_log("wss_init\n");

        ad1848->lark = lark;
                        
        ad1848->status = 0xcc;
        ad1848->index = ad1848->trd = 0;
        ad1848->mce = 0x40;
        
        ad1848->regs[0] = ad1848->regs[1] = 0;
        ad1848->regs[2] = ad1848->regs[3] = 0x80;
        ad1848->regs[4] = ad1848->regs[5] = 0x80;
        ad1848->regs[6] = ad1848->regs[7] = 0x80;
        ad1848->regs[8] = 0;
        ad1848->regs[9] = 0x08;
        ad1848->regs[10] = ad1848->regs[11] = 0;
        ad1848->regs[12] = 0xa;
        ad1848->regs[13] = 0;
        ad1848->regs[14] = ad1848->regs[15] = 0;
        
        ad1848->first_poll = 1;
        
        for (c = 0; c < 64; c++)
        {
                attenuation = 0.0;
                if (c & 0x01) attenuation -= 1.5;
                if (c & 0x02) attenuation -= 3.0;
                if (c & 0x04) attenuation -= 6.0;
                if (c & 0x08) attenuation -= 12.0;
                if (c & 0x10) attenuation -= 24.0;
                if (c & 0x20) attenuation -= 48.0;
                
                attenuation = pow(10, attenuation / 10);
                
                wss_vols[c] = (int)(attenuation * 65536);
//                lark_log("wss_vols %i = %f %i\n", c, attenuation, wss_vols[c]);
        }
        
        ad1848->dma_read = dma_read;
        ad1848->dma_write = dma_write;

        output_iir_gen_coefficients(264600, 20000.0, ad1848->output_iir_coef_a, ad1848->output_iir_coef_b);
}

void ad1848_in_buffer(ad1848_t *ad1848, int16_t *buffer, int samples)
{
        if (!ad1848->in_buffer_pending_samples)
        {
                if (samples > 4800)
                        samples = 4800;
                memcpy(ad1848->in_buffer_raw, buffer, samples*2*2);
//                lark_log("in_buffer %i\n", samples);
                ad1848->in_buffer_pending_samples = samples;
        }
}

static void ad1848_process_in_buffer(ad1848_t *ad1848, int16_t *buffer, int samples)
{
        int samples_avail = (8192*2*2) - (ad1848->in_wp - ad1848->in_rp);
        int samples_to_process;
        int prev_in_f_wp = ad1848->in_f_wp;
        int32_t in_ip_target;
        int c, d = 0, e;
        uint32_t last_read = 0;

//        lark_log("ad1848_in_buffer: %i samples, %i available\n", samples, samples_avail);
//        lark_log("  f_wp=%05x\n", ad1848->in_f_wp);

        d = ad1848->in_f_wp & 0x3ffe0;
        for (c = 0; c < samples; c++)
        {
                ad1848->filter_buffer[d++] = buffer[c*2];
                ad1848->filter_buffer[d++] = buffer[c*2 + 1];
                for (e = 1; e < 16; e++)
                {
                        ad1848->filter_buffer[d++] = 0;
                        ad1848->filter_buffer[d++] = 0;
                }
                d &= 0x3ffe0;
        }

        d = ad1848->in_f_wp & 0x3ffe0;
        for (c = 0; c < samples; c++)
        {
                for (e = 0; e < 16; e++)
                {
                        ad1848->filter_buffer[d] = input_fir(ad1848, 0, ad1848->filter_buffer[d]);
                        d++;
                        ad1848->filter_buffer[d] = input_fir(ad1848, 1, ad1848->filter_buffer[d]);
                        d++;
                }
                d &= 0x3ffe0;
        }
        ad1848->in_f_wp = d;
//        lark_log("  f_wp now=%05x %05x\n", ad1848->in_f_wp, samples*32);
        
        if (samples*2 > samples_avail)
                samples_to_process = samples_avail/2;
        else
                samples_to_process = samples;

        in_ip_target = ((ad1848->in_f_wp - prev_in_f_wp) & 0x3ffe0) << 13;
//        lark_log("  Processing %i samples\n", samples_to_process);
//        lark_log("  in_wp=%04x  in_ip=%08x %05x %05x in_ip_target=%08x\n", ad1848->in_wp, ad1848->in_ip, ad1848->in_ip >> 15, (ad1848->in_ip >> 15) * 2, in_ip_target);
        for (c = 0; c < samples_to_process; c++)
        {
                in_ip_target -= (ad1848->in_i_inc >> 1);
                if (in_ip_target <= 0)
                        break;
                last_read = ad1848->in_ip;
                ad1848->in_buffer[ad1848->in_wp & 0x7fff] = ad1848->filter_buffer[(ad1848->in_ip >> 15) * 2];
                ad1848->in_wp++;
                ad1848->in_buffer[ad1848->in_wp & 0x7fff] = ad1848->filter_buffer[(ad1848->in_ip >> 15) * 2 + 1];
                ad1848->in_wp++;
                ad1848->in_ip += ad1848->in_i_inc;
        }
//        lark_log("  last_read=%08x %05x  c=%i  inc=%08x\n", last_read, (last_read >> 15) * 2, c, ad1848->in_i_inc);
        while (1)
        {
                in_ip_target -= (ad1848->in_i_inc >> 1);
                if (in_ip_target <= 0)
                        break;
                ad1848->in_ip += ad1848->in_i_inc;
        }
}

int wss_irq(ad1848_t *ad1848)
{
        ad1848->status |= 0x01;
        return ad1848->regs[0xa] & 2;
}
