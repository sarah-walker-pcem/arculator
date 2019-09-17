#ifndef _VIDC_H_
#define _VIDC_H_

extern int vidc_displayon;

extern void vidc_redovideotiming();
extern void vidc_setclock(int clock);
extern int vidc_getclock();
int vidc_get_hs();
extern void closevideo();

extern int vidc_framecount;
extern int vidc_dma_length;

void vidc_reset();




typedef struct
{
        int w, h;
        uint8_t *dat;
        uint8_t *line[0];
} BITMAP;

extern BITMAP *screen;

BITMAP *create_bitmap(int w, int h);

typedef struct
{
        uint8_t r, g, b;
} RGB;
        
typedef RGB PALETTE[256];

#define makecol(r, g, b)    ((b) | ((g) << 8) | ((r) << 16))
#define makecol32(r, g, b)  ((b) | ((g) << 8) | ((r) << 16))

#endif /* _VIDC_H_ */
