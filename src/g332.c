/*Arculator 2.1 by Sarah Walker
  Inmos G332/G335 emulation*/
#include <string.h>
#include "arc.h"
#include "g332.h"
#include "plat_video.h"
#include "vidc.h"

#define G332_REG_BOOT            0x00

#define G332_REG_HALF_SYNC       0x21
#define G332_REG_BACK_PORCH      0x22
#define G332_REG_DISPLAY         0x23
#define G332_REG_SHORT_DISPLAY   0x24
#define G332_REG_BROAD_PULSE     0x25
#define G332_REG_VSYNC           0x26
#define G332_REG_V_PRE_EQUALISE  0x27
#define G332_REG_V_POST_EQUALISE 0x28
#define G332_REG_V_BLANK         0x29
#define G332_REG_V_DISPLAY       0x2a
#define G332_REG_LINE_TIME       0x2b
#define G332_REG_LINE_START      0x2c
#define G332_REG_MEM_INIT        0x2d
#define G332_REG_TRANSFER_DELAY  0x2e

#define G332_REG_CTRL_A          0x60
#define G332_REG_CTRL_B          0x70
#define G332_TOP_OF_SCREEN       0x80
#define G332_CURSOR_PALETTE_1    0xa1
#define G332_CURSOR_PALETTE_2    0xa2
#define G332_CURSOR_PALETTE_3    0xa3
#define G332_CURSOR_POSITION     0xc7

#define G332_BOOT_MULTIPLIER_MASK (0x1f << 0)
#define G332_BOOT_PLL             (1 << 5)

#define G332_CTRL_INTERLACE      (1 << 1)
#define G332_CTRL_BPP_MASK       (7 << 20)
#define G332_CTRL_BPP_1          (0 << 20)
#define G332_CTRL_BPP_2          (1 << 20)
#define G332_CTRL_BPP_4          (2 << 20)
#define G332_CTRL_BPP_8          (3 << 20)
#define G332_CTRL_BPP_15         (4 << 20)
#define G332_CTRL_BPP_16         (5 << 20)
#define G332_CTRL_CURSOR_DISABLE (1 << 23)

enum
{
	V_STATE_BLANKING = 0,
	V_STATE_DISPLAY,
	V_STATE_PRE_EQUALISE,
	V_STATE_VSYNC,
	V_STATE_POST_EQUALISE,

	V_STATE_LAST
};

void g332_init(g332_t *g332, uint8_t *ram, int type, void (*irq_callback)(void *p, int state), void *callback_p)
{
	memset(g332, 0, sizeof(g332_t));
	g332->type = type;
	g332->ram = ram;
	g332->irq_callback = irq_callback;
	g332->callback_p = callback_p;
	g332->buffer = create_bitmap(2048,2048);
}

void g332_close(g332_t *g332)
{
	destroy_bitmap(g332->buffer);
}

void g332_output_enable(g332_t *g332, int enable)
{
	g332->output_enable = enable;
}


uint64_t g332_poll(g332_t *g332)
{
	if (g332->output_enable)
	{
		if (g332->v_state == V_STATE_DISPLAY && g332->line < 2048)
		{
			int display_line;
			uint32_t *p;
			int x;
			int cursor_line;

			if (g332->ctrl_a & G332_CTRL_INTERLACE)
				display_line = g332->line*2 + (g332->interlace_ff ? 1 : 0);
			else
				display_line = g332->line;
			p = (uint32_t *)g332->buffer->line[display_line];

			switch (g332->ctrl_a & G332_CTRL_BPP_MASK)
			{
				case G332_CTRL_BPP_1:
				for (x = 0; x < g332->h_display; x += 8)
				{
					uint8_t data = g332->ram[g332->rp & 0x7ffff];

					*p++ = g332->palette[data & 1];
					*p++ = g332->palette[(data >> 1) & 1];
					*p++ = g332->palette[(data >> 2) & 1];
					*p++ = g332->palette[(data >> 3) & 1];
					*p++ = g332->palette[(data >> 4) & 1];
					*p++ = g332->palette[(data >> 5) & 1];
					*p++ = g332->palette[(data >> 6) & 1];
					*p++ = g332->palette[(data >> 7) & 1];

					g332->rp++;
				}
				if (g332->ctrl_a & G332_CTRL_INTERLACE)
					g332->rp += g332->h_display/8;
				break;
				case G332_CTRL_BPP_2:
				for (x = 0; x < g332->h_display; x += 4)
				{
					uint8_t data = g332->ram[g332->rp & 0x7ffff];

					*p++ = g332->palette[data & 3];
					*p++ = g332->palette[(data >> 2) & 3];
					*p++ = g332->palette[(data >> 4) & 3];
					*p++ = g332->palette[(data >> 6) & 3];

					g332->rp++;
				}
				if (g332->ctrl_a & G332_CTRL_INTERLACE)
					g332->rp += g332->h_display/4;
				break;
				case G332_CTRL_BPP_4:
				for (x = 0; x < g332->h_display; x += 2)
				{
					uint8_t data = g332->ram[g332->rp & 0x7ffff];

					*p++ = g332->palette[data & 0xf];
					*p++ = g332->palette[data >> 4];

					g332->rp++;
				}
				if (g332->ctrl_a & G332_CTRL_INTERLACE)
					g332->rp += g332->h_display/2;
				break;
				case G332_CTRL_BPP_8:
				for (x = 0; x < g332->h_display; x++)
				{
					uint8_t data = g332->ram[g332->rp & 0x7ffff];

					*p++ = g332->palette[data];

					g332->rp++;
				}
				if (g332->ctrl_a & G332_CTRL_INTERLACE)
					g332->rp += g332->h_display;
				break;
				case G332_CTRL_BPP_15:
				for (x = 0; x < ((g332->type == INMOS_G332) ? g332->h_display : (g332->h_display/2)); x++)
				{
					uint16_t data = *(uint16_t *)&g332->ram[g332->rp & 0x7ffff];
					uint32_t data_32 = ((data & 0x1f) << 19) | ((data & 0x3e0) << 6) | ((data & 0x7c00) >> 7);

					*p++ = data_32;

					g332->rp += 2;
				}
				if (g332->ctrl_a & G332_CTRL_INTERLACE)
					g332->rp += g332->h_display;
				break;
				case G332_CTRL_BPP_16:
				for (x = 0; x < ((g332->type == INMOS_G332) ? g332->h_display : (g332->h_display/2)); x++)
				{
					uint16_t data = *(uint16_t *)&g332->ram[g332->rp & 0x7ffff];
					uint32_t data_32 = ((data & 0x1f) << 19) | ((data & 0x7e0) << 5) | ((data & 0xf800) >> 8);

					*p++ = data_32;

					g332->rp += 2;
				}
				if (g332->ctrl_a & G332_CTRL_INTERLACE)
					g332->rp += g332->h_display;
				break;
			}

			cursor_line = display_line - g332->cursor_y;
			if (cursor_line >= 0 && cursor_line < 64 && !(g332->ctrl_a & G332_CTRL_CURSOR_DISABLE))
			{
				int cursor_row = (g332->cursor_x >= 0) ? 0 : -g332->cursor_x;
				uint64_t cursor_data = *(uint64_t *)&g332->cursor_store[cursor_line*8];
				int cursor_x;
				uint32_t *p;

				if ((g332->ctrl_a & G332_CTRL_BPP_MASK) >= G332_CTRL_BPP_15)
					cursor_x = ((g332->cursor_x + 21) / 2) - 21; /*Horrible bodge and probably wrong*/
				else
					cursor_x = g332->cursor_x;

				p = &((uint32_t *)g332->buffer->line[display_line])[(cursor_x >= 0) ? cursor_x : 0];
				cursor_data >>= cursor_row*2;

				while (cursor_row < 64)
				{
					if (cursor_data & 3)
						*p = g332->cursor_palette[cursor_data & 3];
					p++;
					cursor_data >>= 2;
					cursor_row++;
				}
			}

			g332->line++;
		}
	}

	g332->v_count -= 2;
	if (g332->v_count <= 0)
	{
		g332->v_state++;
		if (g332->v_state >= V_STATE_LAST)
			g332->v_state = V_STATE_BLANKING;

		switch (g332->v_state)
		{
			case V_STATE_BLANKING:
			g332->v_count = g332->v_blank;
			break;

			case V_STATE_DISPLAY:
			g332->v_count = g332->v_display;
			g332->line = 0;
			g332->rp = 0;

			if ((g332->ctrl_a & G332_CTRL_INTERLACE) && g332->interlace_ff)
			{
				switch (g332->ctrl_a & G332_CTRL_BPP_MASK)
				{
					case G332_CTRL_BPP_1:
					g332->rp += g332->h_display/8;
					break;
					case G332_CTRL_BPP_2:
					g332->rp += g332->h_display/4;
					break;
					case G332_CTRL_BPP_4:
					g332->rp += g332->h_display/2;
					break;
					case G332_CTRL_BPP_8:
					g332->rp += g332->h_display;
					break;
					case G332_CTRL_BPP_15:
					g332->rp += g332->h_display;
					break;
					case G332_CTRL_BPP_16:
					g332->rp += g332->h_display;
					break;
				}
			}
			break;

			case V_STATE_PRE_EQUALISE:
			if (g332->output_enable)
			{
				int h_display = ((g332->ctrl_a & G332_CTRL_BPP_MASK) >= G332_CTRL_BPP_15 && g332->type == INMOS_G335) ? g332->h_display/2 : g332->h_display;
				int v_display = (g332->ctrl_a & G332_CTRL_INTERLACE) ? g332->v_display : g332->v_display/2;

				if (((double)h_display / (double)v_display) > (5.0/3.0))
				{
					updatewindowsize(h_display, v_display * 2);
					video_renderer_update(g332->buffer, 0, 0, 0, 0, h_display, v_display);
					video_renderer_present(0, 0, h_display, v_display, 1);
				}
				else
				{
					updatewindowsize(h_display, v_display);
					video_renderer_update(g332->buffer, 0, 0, 0, 0, h_display, v_display);
					video_renderer_present(0, 0, h_display, v_display, 0);
				}
			}

			g332->interlace_ff = !g332->interlace_ff;
			g332->v_count = g332->v_pre_equalise;
			break;

			case V_STATE_VSYNC:
			if (g332->irq_callback)
				g332->irq_callback(g332->callback_p, 1);
			g332->v_count = g332->vsync;
			break;

			case V_STATE_POST_EQUALISE:
			if (g332->irq_callback)
				g332->irq_callback(g332->callback_p, 0);
			g332->v_count = g332->v_post_equalise;
			break;

		}

//                rpclog("g332 now in %i state, v_count=%i\n", g332->v_state, g332->v_count/2);
	}


	return (g332->line_length < (1ull << 32)) ? (32ull << 32) : g332->line_length;
}

void g332_write(g332_t *g332, uint32_t addr, uint32_t val)
{
	if (addr < 0x100)
	{
		switch (addr)
		{
			case G332_REG_BOOT:
			g332->boot = val;
			if (val & G332_BOOT_PLL)
				g332->pixel_clock = 5.0 * (val & G332_BOOT_MULTIPLIER_MASK);
			else
				g332->pixel_clock = 14.7456;
//                        rpclog("G332 boot = %06x\n", val);
			g332->line_length = (int64_t)(((double)(g332->line_time) / g332->pixel_clock) * (double)(1ull << 32)/*TIMER_USEC*/);
//                        rpclog("line_time=%i pixel_clock=%g line_length=%016llx\n", g332->line_time, g332->pixel_clock, g332->line_length);
			break;

			case G332_REG_HALF_SYNC:
			break;

			case G332_REG_BACK_PORCH:
			break;

			case G332_REG_DISPLAY:
			g332->h_display = val*4;
			break;

			case G332_REG_SHORT_DISPLAY:
			break;

			case G332_REG_BROAD_PULSE:
			break;

			case G332_REG_VSYNC:
			g332->vsync = val;
			break;

			case G332_REG_V_PRE_EQUALISE:
			g332->v_pre_equalise = val;
			break;

			case G332_REG_V_POST_EQUALISE:
			g332->v_post_equalise = val;
			break;

			case G332_REG_V_BLANK:
			g332->v_blank = val;
			break;

			case G332_REG_V_DISPLAY:
			g332->v_display = val;
			break;

			case G332_REG_LINE_TIME:
			g332->line_time = val*4;
			g332->line_length = (int64_t)(((double)(g332->line_time) / g332->pixel_clock) * (double)(1ull << 32)/*TIMER_USEC*/);
//                        rpclog("line_time=%i pixel_clock=%g line_length=%016llx %016llx\n", g332->line_time, g332->pixel_clock, g332->line_length, TIMER_USEC);
			break;

			case G332_REG_LINE_START:
			break;

			case G332_REG_MEM_INIT:
			break;

			case G332_REG_TRANSFER_DELAY:
			break;

			case G332_REG_CTRL_A:
			g332->ctrl_a = val;
//                        rpclog("g332 ctrl_a = %08x\n", val);
			break;

			case G332_REG_CTRL_B:
			break;

			case G332_TOP_OF_SCREEN:
//                        rpclog("top of screen = %08x\n", val);
			break;

			case G332_CURSOR_PALETTE_1:
			g332->cursor_palette[1] = ((val & 0xff) << 16) | (val & 0x00ff00) | ((val & 0xff0000) >> 16);
//                        rpclog("cursor_palette[1]=%06x %06x\n", g332->cursor_palette[1], val);
			break;
			case G332_CURSOR_PALETTE_2:
			g332->cursor_palette[2] = ((val & 0xff) << 16) | (val & 0x00ff00) | ((val & 0xff0000) >> 16);
//                        rpclog("cursor_palette[2]=%06x %06x\n", g332->cursor_palette[2], val);
			break;
			case G332_CURSOR_PALETTE_3:
			g332->cursor_palette[3] = ((val & 0xff) << 16) | (val & 0x00ff00) | ((val & 0xff0000) >> 16);
//                        rpclog("cursor_palette[3]=%06x %06x\n", g332->cursor_palette[3], val);
			break;

			case G332_CURSOR_POSITION:
			g332->cursor_pos = val;
			g332->cursor_x = ((int32_t)val << 8) >> 20;
			g332->cursor_y = ((int32_t)val << 20) >> 20;
//                        rpclog("cursor_position: %08x %i,%i\n", val, g332->cursor_x, g332->cursor_y);
			break;
		}
	}
	else if (addr >= 0x100 && addr < 0x200)
	{
		g332->palette[addr & 0xff] = ((val & 0xff) << 16) | (val & 0x00ff00) | ((val & 0xff0000) >> 16);
//                rpclog("palette[%02x]=%06x\n", addr & 0xff, val);
	}
	else if (addr >= 0x200 && addr < 0x400)
		g332->cursor_store[addr & 0x1ff] = val & 0xffff;
}
