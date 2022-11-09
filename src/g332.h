//#include "timer.h"
#include "vidc.h"

typedef struct g332_t
{
	int type;

	uint32_t boot;

	uint32_t ctrl_a;

	int output_enable;

	uint32_t v_blank, v_display, v_pre_equalise, vsync, v_post_equalise;
	uint32_t h_display;
	uint32_t cursor_pos;
	uint32_t line_time;

	int v_state, v_count;
	int line;
	int cursor_x, cursor_y;
	int interlace_ff;

	double pixel_clock;
	uint64_t line_length;

	uint8_t *ram;
	uint32_t rp;

	uint32_t palette[256];
	uint32_t cursor_palette[4];
	uint16_t cursor_store[512];

	BITMAP *buffer;

	void (*irq_callback)(void *p, int state);
	void *callback_p;
} g332_t;

enum
{
	INMOS_G332,
	INMOS_G335
};

void g332_init(g332_t *g332, uint8_t *ram, int type, void (*irq_callback)(void *p, int state), void *callback_p);
void g332_close(g332_t *g332);

void g332_write(g332_t *g332, uint32_t addr, uint32_t val);
void g332_output_enable(g332_t *g332, int enable);
uint64_t g332_poll(g332_t *g332);
