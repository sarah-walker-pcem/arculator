/*Much of this is guesswork. Acorn didn't publically document the LC ASIC in any
  real detail, so this is based on what information is present in the A4 TERM and
  RO 3.71 source*/
#include "arc.h"
#include "config.h"
#include "ioeb.h"
#include "lc.h"
#include "timer.h"
#include "vidc.h"
#include "plat_video.h"

/*for mode 27, VDSR=1ff, VDLR=ed, HDSR=6e4, HDLR=50*/
/*LICR=7d2*/
/*Nominal video timings - pixel rate 24 MHz, horizontal 1192 pixels, vertical 485 lines, H 20.134Hz, V 41.51Hz*/

/*VDSR = 512 - lines from start of vsync to start of display*/
#define LC_VDSR_L  0x00
#define LC_VDSR_M  0x04
#define LC_VDSR_H  0x08

/*VDLC = lines per panel - 3*/
#define LC_VDLR_L  0x0c
#define LC_VDLR_M  0x10
#define LC_VDLR_H  0x14

/*HDSR = 2047 - pixels of hsync + back porch*/
#define LC_HDSR_L  0x18
#define LC_HDSR_M  0x1c
#define LC_HDSR_H  0x20

/*HDLR = horizontal display pixels / 8*/
#define LC_HDLR_L  0x24
#define LC_HDLR_M  0x28
#define LC_HDLR_H  0x2c

#define LC_LICR_L  0x30
#define LC_LICR_M  0x34
#define LC_LICR_H  0x38

#define LC_RESET   0x3c

#define LICR_CLOCK_MASK  (3 << 5)
#define LICR_CLOCK_IOEB  (0 << 5) /*VIDC using IOEB clock*/
#define LICR_CLOCK_CRYS2 (1 << 5) /*VIDC using crystal/2 (12 MHz)*/
#define LICR_CLOCK_CRYS  (2 << 5) /*VIDC using crystal (24 MHz)*/

static struct
{
	/*Real A4 uses 2x 64kx4 to store post-dithered 1 bpp image (or so I
	  believe). As we don't perform LC dithering, quadruple the array and
	  store 4 bpp instead*/
	uint8_t ram[0x10000 * 4];

	uint32_t wp;

	uint32_t vdsr, vdlr, hdsr, hdlr;
	uint32_t licr;

	int v_delay, v_display, vc;

	uint32_t pal[16];

	BITMAP *lc_buffer;

	int has_updated;
	emu_timer_t blank_timer;
} lc;

/*Grey levels taken from patent GB 2245743*/
static const uint32_t lc_palette[16] =
{
	0x000000,
	0x1c1c1c, //11.5%, 28
	0x333333, //20.0%, 51
	0x444444, //26.7%, 68
	0x555555, //33.3%, 85
	0x666666, //40.0%, 102
	0x717171, //44.4%, 113
	0x808080, //50.0%, 128

	0xffffff, //100.0%, 255
	0xe3e3e3, //88.9%, 227
	0xcccccc, //80.0%, 204
	0xbbbbbb, //73.3%, 187
	0xaaaaaa, //66.7%, 170
	0x999999, //60.0%, 153
	0x8e8e8e, //55.6%, 142
	0x808080, //50.0%, 128
};

static void lc_vidc_data(uint8_t *data, int pixels, int hsync_length, int resolution, void *p);
static void lc_vidc_vsync(void *p, int state);
static void blank_timer_callback(void *p);

void lc_init(void)
{
	lc.vdsr = 0;
	lc.vdlr = 0;
	lc.hdsr = 0;
	lc.hdlr = 0;
	lc.licr = 0;
	if (!lc.lc_buffer)
		lc.lc_buffer = create_bitmap(640, 480);
	vidc_attach(lc_vidc_data, lc_vidc_vsync, NULL);

	if (monitor_type == MONITOR_LCD)
	{
		vidc_output_enable(0);

		updatewindowsize(640, 480);
		video_renderer_update(lc.lc_buffer, 0, 0, 0, 0, 640, 480);
		video_renderer_present(0, 0, 640, 480, 0);

		timer_add(&lc.blank_timer, blank_timer_callback, NULL, 1);
	}
}

static void lc_vidc_data(uint8_t *data, int pixels, int hsync_length, int resolution, void *p)
{
	int c;

	if (lc.v_delay)
	{
		lc.v_delay--;
		return;
	}

	if (lc.vc < lc.v_display*2)
	{
		/*Copy data from VIDC*/
		if ((lc.licr & LICR_CLOCK_MASK) != LICR_CLOCK_CRYS2)
		{
			int pixels_to_copy = MIN(pixels, lc.hdlr*8);
			int h_offset = 2047 - lc.hdsr;

//                rpclog("LC %03i: write %05x pixels_to_copy=%i\n", lc.vc, lc.wp, pixels_to_copy);
			for (c = 0; c < pixels_to_copy; c += 2)
			{
				lc.ram[lc.wp] = data[c+h_offset] | (data[c+h_offset+1] << 4);
				lc.wp = (lc.wp + 1) & 0x3ffff;
			}
		}
		else
		{
			/*Pixel doubling*/
			int pixels_to_copy = MIN(pixels, lc.hdlr*4);
			int h_offset = (2047 - lc.hdsr)/2;

//                        rpclog("LC double %03i: write %05x pixels_to_copy=%i\n", lc.vc, lc.wp, pixels_to_copy);
			for (c = 0; c < pixels_to_copy; c += 2)
			{
				lc.ram[lc.wp] = data[c+h_offset] | (data[c+h_offset] << 4);
				lc.wp = (lc.wp + 1) & 0x3ffff;
				lc.ram[lc.wp] = data[c+h_offset+1] | (data[c+h_offset+1] << 4);
				lc.wp = (lc.wp + 1) & 0x3ffff;
			}
		}
	}

	lc.vc++;
	if (lc.vc == lc.v_display || lc.vc == lc.v_display*2)
	{
		uint32_t rp = 0;
		int y;

		for (y = 0; y < 480; y++)
		{
			uint32_t *wp = (uint32_t *)lc.lc_buffer->line[y];
			int width = MIN(640, lc.hdlr*8);

//                        rpclog("LC %03i: read %05x width=%i\n", lc.vc, lc.rp, width);
			for (c = 0; c < width; c += 2)
			{
				uint8_t data = lc.ram[rp];
				rp = (rp + 1) & 0x3ffff;

				*wp++ = lc.pal[data & 0xf];
				*wp++ = lc.pal[data >> 4];
			}
		}

		if (monitor_type == MONITOR_LCD)
		{
			updatewindowsize(640, 480);
			video_renderer_update(lc.lc_buffer, 0, 0, 0, 0, 640, 480);
			video_renderer_present(0, 0, 640, 480, 0);
			lc.has_updated = 1;
		}
	}
}

static void lc_vidc_vsync(void *p, int state)
{
	if (state)
	{
		lc.wp = 0;

		lc.vc = 0;
		lc.v_delay = 512 - lc.vdsr;
		lc.v_display = lc.vdlr + 3;
	}
//        rpclog("LC: v_delay=%i v_display=%i hdlr=%i\n", lc.v_delay, lc.v_display, lc.hdlr);
}

/*If LCD hasn't updated the screen for a while, blank it. Avoids artifacts
  during initial boot*/
static void blank_timer_callback(void *p)
{
	timer_advance_u64(&lc.blank_timer, TIMER_USEC * 100 * 1000);

	if (!lc.has_updated)
	{
		clear(lc.lc_buffer);

		updatewindowsize(640, 480);
		video_renderer_update(lc.lc_buffer, 0, 0, 0, 0, 640, 480);
		video_renderer_present(0, 0, 640, 480, 0);
	}
	else
		lc.has_updated = 0;
}

uint8_t lc_read(uint32_t addr)
{
	uint8_t ret = 0xff;

	switch (addr & 0x7c)
	{
		case LC_LICR_L:
		ret = lc.licr & 0xf;
		break;
		case LC_LICR_M:
		ret = (lc.licr >> 4) & 0xf;
		break;
		case LC_LICR_H:
		ret = (lc.licr >> 8) & 0xf;
		break;

		case LC_RESET:
		ret = 4;
		break;
	}

//        rpclog("lc_read: addr=%07x val=%02x PC=%07x\n", addr, ret, PC);

	return ret;
}

void lc_write(uint32_t addr, uint8_t val)
{
	switch (addr & 0x7c)
	{
		case LC_VDSR_L:
		lc.vdsr = (lc.vdsr & 0xff0) | (val & 0xf);
		rpclog("LC VDSR=%03x\n", lc.vdsr);
		break;
		case LC_VDSR_M:
		lc.vdsr = (lc.vdsr & 0xf0f) | ((val & 0xf) << 4);
		rpclog("LC VDSR=%03x\n", lc.vdsr);
		break;
		case LC_VDSR_H:
		lc.vdsr = (lc.vdsr & 0x0ff) | ((val & 0xf) << 8);
		rpclog("LC VDSR=%03x\n", lc.vdsr);
		break;

		case LC_VDLR_L:
		lc.vdlr = (lc.vdlr & 0xff0) | (val & 0xf);
		rpclog("LC VDLR=%03x\n", lc.vdlr);
		break;
		case LC_VDLR_M:
		lc.vdlr = (lc.vdlr & 0xf0f) | ((val & 0xf) << 4);
		rpclog("LC VDLR=%03x\n", lc.vdlr);
		break;
		case LC_VDLR_H:
		lc.vdlr = (lc.vdlr & 0x0ff) | ((val & 0xf) << 8);
		rpclog("LC VDLR=%03x\n", lc.vdlr);
		break;

		case LC_HDSR_L:
		lc.hdsr = (lc.hdsr & 0xff0) | (val & 0xf);
		rpclog("LC HDSR=%03x\n", lc.hdsr);
		break;
		case LC_HDSR_M:
		lc.hdsr = (lc.hdsr & 0xf0f) | ((val & 0xf) << 4);
		rpclog("LC HDSR=%03x\n", lc.hdsr);
		break;
		case LC_HDSR_H:
		lc.hdsr = (lc.hdsr & 0x0ff) | ((val & 0xf) << 8);
		rpclog("LC HDSR=%03x\n", lc.hdsr);
		break;

		case LC_HDLR_L:
		lc.hdlr = (lc.hdlr & 0xff0) | (val & 0xf);
		rpclog("LC HDLR=%03x\n", lc.hdlr);
		break;
		case LC_HDLR_M:
		lc.hdlr = (lc.hdlr & 0xf0f) | ((val & 0xf) << 4);
		rpclog("LC HDLR=%03x\n", lc.hdlr);
		break;
		case LC_HDLR_H:
		lc.hdlr = (lc.hdlr & 0x0ff) | ((val & 0xf) << 8);
		rpclog("LC HDLR=%03x\n", lc.hdlr);
		break;

		case LC_LICR_L:
		lc.licr = (lc.licr & 0xff0) | (val & 0xf);
		rpclog("LC LICR=%03x\n", lc.licr);
		break;
		case LC_LICR_M:
		lc.licr = (lc.licr & 0xf0f) | ((val & 0xf) << 4);
		rpclog("LC LICR=%03x\n", lc.licr);
		switch (lc.licr & LICR_CLOCK_MASK)
		{
			case LICR_CLOCK_CRYS:
			vidc_setclock_direct(24000);
			break;
			case LICR_CLOCK_CRYS2:
			vidc_setclock_direct(12000);
			break;
			case LICR_CLOCK_IOEB:
			default:
			vidc_setclock(ioeb_clock_select);
			break;
		}
		break;
		case LC_LICR_H:
		lc.licr = (lc.licr & 0x0ff) | ((val & 0xf) << 8);
		rpclog("LC LICR=%03x\n", lc.licr);
		break;
	}
	if (addr & 0x40)
		lc.pal[(addr >> 2) & 0xf] = lc_palette[val & 0xf];
	//rpclog("lc_write: addr=%07x val=%02x PC=%07x\n", addr, val, PC);
}
