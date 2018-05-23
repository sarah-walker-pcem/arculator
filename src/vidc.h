extern int vidc_displayon;

void vidc_redovideotiming();
void vidc_setclock(int clock);
int vidc_getclock();
int vidc_update_cycles();
int vidcgetcycs();
extern int vidc_framecount;
extern int vidc_dma_length;

void closevideo();






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
