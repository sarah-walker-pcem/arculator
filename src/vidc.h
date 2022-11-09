#ifndef _VIDC_H_
#define _VIDC_H_

extern int vidc_displayon;

extern void vidc_redovideotiming();
extern void vidc_setclock(int clock);
void vidc_setclock_direct(int clock);
extern int vidc_getclock();
int vidc_get_hs();
extern void closevideo();
/*Attach device to VIDC extenal data port. vidc_data() provides device with a
  scanline's worth of pixels. vidc_vsync() indicates the start of VIDC's vsync*/
void vidc_attach(void (*vidc_data)(uint8_t *data, int pixels, int hsync_length, int resolution, void *p), void (*vidc_vsync)(void *p, int state), void *p);
/*Enable VIDC output. Set to 0 if another device is driving the screen*/
void vidc_output_enable(int ena);

extern int vidc_framecount;
extern int vidc_dma_length;

void vidc_reset();

uint32_t vidc_get_current_vaddr(void);
uint32_t vidc_get_current_caddr(void);

void vidc_debug_print(char *s);


typedef struct
{
	int w, h;
	uint8_t *dat;
	uint8_t *line[0];
} BITMAP;

extern BITMAP *screen;

BITMAP *create_bitmap(int w, int h);
void destroy_bitmap(BITMAP *b);

typedef struct
{
	uint8_t r, g, b;
} RGB;

typedef RGB PALETTE[256];

#define makecol(r, g, b)    ((b) | ((g) << 8) | ((r) << 16))
#define makecol32(r, g, b)  ((b) | ((g) << 8) | ((r) << 16))

void clear(BITMAP *b);

#endif /* _VIDC_H_ */
