/*Arculator 0.8 by Tom Walker
  VIDC10 emulation*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if WIN32
#define BITMAP __win_BITMAP
#include <windows.h>
#undef BITMAP
#endif
#include "arc.h"
#include "arm.h"
#include "ioc.h"
#include "keyboard.h"
#include "mem.h"
#include "memc.h"
#include "sound.h"
#include "timer.h"
#include "vidc.h"
#include "video.h"
#include "plat_video.h"

/*RISC OS 3 sets a total of 832 horizontal and 288 vertical for MODE 12. We use
  768x576 to get a 4:3 aspect ratio. This also allows MODEs 33-36 to display
  correctly*/
#define TV_X_MIN (197)
#define TV_X_MAX (TV_X_MIN+768)
#define TV_Y_MIN (6)
#define TV_Y_MAX (TV_Y_MIN+288)

/*TV horizontal settings for 12/24 MHz modes. Use 1152x576, scaled to 4:3*/
#define TV_X_MIN_24 (295)
#define TV_X_MAX_24 (TV_X_MIN_24+1152)

int display_mode;
int video_fullscreen_scale;
int video_linear_filtering;
int video_window_resizeable;

static uint32_t vidcr[64];

/* VIDC Control Register
   Bits 31-16: 11100000 XXXXXXXX
   Bits 15-14: Test mode (00 normal, 01 test mode 0, 10 test mode 1, 11 test mode 2)
   Bits 13-9: XXXXX
   Bit 8: Test mode (0 normal, 1 test mode 3)
   Bit 7: Composite sync (0 vsync, 1 csync)
   Bit 6: Interlace sync (0 interlace off, 1 interlace on)
   Bits 5-4: DMA request (00 end of word 0,4, 01 end of word 1,5, 10 end of word 2,6, 11 end of word 3,7)
   Bits 3-2: Bits per pixel (00 1bpp, 01 2bpp, 10 4bpp, 11 8bpp)
   Bits 1-0: Pixel rate (00 8MHz, 01 12MHz, 10 16MHz, 11 24MHz)
*/
#define VIDC_CR 0x38

static int soundhz;
int soundper;

int offsetx = 0, offsety = 0;
int fullscreen;
int fullborders,noborders;
int hires;
int dblscan;


BITMAP *create_bitmap(int x, int y)
{
        BITMAP *b = (BITMAP *)malloc(sizeof(BITMAP) + (y * sizeof(uint8_t *)));
        int c;
        b->dat = (uint8_t *)malloc(x * y * 4);
        for (c = 0; c < y; c++)
        {
                b->line[c] = b->dat + (c * x * 4);
        }
        b->w = x;
        b->h = y;
        return b;
}

void destroy_bitmap(BITMAP *b)
{
        free(b->dat);
        free(b);
}

static void clear(BITMAP *b)
{
        memset(b->dat, 0, b->w * b->h * 4);
}

int vidc_dma_length;
extern int vidc_fetches;
int vidc_framecount = 0;
int vidc_displayon = 0;
int blitcount=0;
/*b - memory buffer*/
BITMAP *buffer;

int flyback;
int deskdepth;
int videodma=0;
int palchange;
uint32_t vidlookup[256];   /*Lookup table for 4bpp modes*/

int redrawpalette=0;

int oldflash;


struct
{
        uint32_t vtot,htot,vsync;
        int line;
        int displayon,borderon;
        uint32_t addr,caddr;
        int vbstart,vbend;
        int vdstart,vdend;
        int hbstart,hbend;
        int hdstart,hdend;
        int hdstart2,hdend2;
        uint32_t cr;
        int sync,inter;
        /*Palette lookups - pal8 for 8bpp modes, pal for all others*/
        uint32_t pal[32],pal8[256];
        int cx,cys,cye,cxh;
        int scanrate;
        
        int in_display;
        int cyclesperline_display, cyclesperline_blanking;
        int cycles_per_fetch;
        int fetch_count;
        
        int clock;
        
        int disp_len, disp_rate, disp_count;
        
        int disp_y_min, disp_y_max;
        int y_min, y_max;
        
        int border_was_disabled, display_was_disabled;
        
        timer_t timer;
} vidc;

int vidc_getline()
{
//        if (vidc.scanrate) return vidc.line>>1;
        return vidc.line;
}
uint32_t monolook[16][4];
uint32_t hirescurcol[4]={0,0,0,0xFFFFFF};

void redolookup()
{
        int c;
        if (hires)
        {
                for (c=0;c<16;c++)
                {
                        monolook[c][0]=(vidcr[c]&1)?0xFFFFFF:0x000000;
                        monolook[c][1]=(vidcr[c]&2)?0xFFFFFF:0x000000;
                        monolook[c][2]=(vidcr[c]&4)?0xFFFFFF:0x000000;
                        monolook[c][3]=(vidcr[c]&8)?0xFFFFFF:0x000000;
                }
        }
        switch (vidcr[VIDC_CR]&0xF) /*Control Register*/
        {
                case 2: /*Mode 0*/
                case 3: /*Mode 25*/
                vidlookup[0]=vidc.pal[0]|(vidc.pal[0]<<16);
                vidlookup[1]=vidc.pal[1]|(vidc.pal[0]<<16);
                vidlookup[2]=vidc.pal[0]|(vidc.pal[1]<<16);
                vidlookup[3]=vidc.pal[1]|(vidc.pal[1]<<16);
                break;
                case 6: /*Mode 8*/
                case 7: /*Mode 26*/
                for (c=0;c<16;c++)
                {
                        vidlookup[c]=vidc.pal[c&0x3]|(vidc.pal[(c>>2)&0x3]<<16);
                }
                break;
                case 8: /*Mode 9*/
                case 9: /*Mode 48*/
                for (c=0;c<16;c++)
                {
                        vidlookup[c]=vidc.pal[c&0xF]|(vidc.pal[c&0xF]<<16);
                }
                break;
                case 10: /*Mode 12*/
                case 11: /*Mode 27*/
                for (c=0;c<256;c++)
                {
                        vidlookup[c]=vidc.pal[c&0xF]|(vidc.pal[(c>>4)&0xF]<<16);
                }
                break;
        }
}

void recalcse()
{
        if (hires)
        {
                        vidc.hdstart=(vidc.hdstart2<<1)-14;
                        vidc.hdend=(vidc.hdend2<<1)-14;
        }
        else
        {
                switch (vidcr[0xE0>>2]&0xC)
                {
                        case 0xC: /*8bpp*/
                        vidc.hdstart=(vidc.hdstart2<<1)+5;
                        vidc.hdend=(vidc.hdend2<<1)+5;
                        break;
                        case 8: /*4bpp*/
                        vidc.hdstart=(vidc.hdstart2<<1)+7;
                        vidc.hdend=(vidc.hdend2<<1)+7;
                        break;
                        case 4: /*2bpp*/
                        vidc.hdstart=(vidc.hdstart2<<1)+11;
                        vidc.hdend=(vidc.hdend2<<1)+11;
                        break;
                        case 0: /*1bpp*/
                        vidc.hdstart=(vidc.hdstart2<<1)+19;
                        vidc.hdend=(vidc.hdend2<<1)+19;
                        break;
                }
        }
}

void writevidc(uint32_t v)
{
//        char s[80];
        RGB r;
        int c,d;
        LOG_VIDC_REGISTERS("Write VIDC %08X (addr %02X<<2 or %02X/%02X, data %06X) with R15=%08X (PC=%08X)\n",
                v, v>>26, v>>24, (v>>24) & 0xFC, v & 0xFFFFFFul, armregs[15], PC);
        if (((v>>24)&~0x1F)==0x60)
        {
                stereoimages[((v>>26)-1)&7]=v&7;
//                rpclog("Stereo image write %08X %i %i\n",v,((v>>26)-1)&7,v&7);
        }
        if (((v>>26)<0x14) && (v!=vidcr[v>>26] || redrawpalette))
        {
/*                if ((v>>26)<0x10) rpclog("Write pal %08X\n", v);
                switch (v >> 26)
                {
                        case  0: v = 0x00000000; break;
                        case  1: v = 0x04000111; break;
                        case  2: v = 0x08000222; break;
                        case  3: v = 0x0C000333; break;
                        case  4: v = 0x10000004; break;
                        case  5: v = 0x14000115; break;
                        case  6: v = 0x18000226; break;
                        case  7: v = 0x1C000337; break;
                        case  8: v = 0x20000400; break;
                        case  9: v = 0x24000511; break;
                        case 10: v = 0x28000622; break;
                        case 11: v = 0x2C000733; break;
                        case 12: v = 0x30000404; break;
                        case 13: v = 0x34000515; break;
                        case 14: v = 0x38000626; break;
                        case 15: v = 0x3C000737; break;
                }*/
                vidcr[v>>26]=v;
                r.b=(v&0xF00)>>6;
                r.g=(v&0xF0)>>2;
                r.r=(v&0xF)<<2;
                c=vidc.pal[(v>>26)&0x1F];
                vidc.pal[(v>>26)&0x1F]=makecol((r.r<<2)|(r.r>>2),(r.g<<2)|(r.g>>2),(r.b<<2)|(r.b>>2));
                LOG_VIDC_REGISTERS("VIDC Write pal %08X %08X %08X\n",c,vidc.pal[(v>>26)&0x1F],v);
                d=v>>26;
                palchange=1;
                for (c=d;c<0x100+d;c+=16)
                {
                        r.r=vidcr[d&15]&0xF;
                        r.g=(vidcr[d&15]&0xF0)>>4;
                        r.b=(vidcr[d&15]&0xF00)>>8;
                        if (c&0x10) r.r|=8; else r.r&=~8;
                        if (c&0x20) r.g|=4; else r.g&=~4;
                        if (c&0x40) r.g|=8; else r.g&=~8;
                        if (c&0x80) r.b|=8; else r.b&=~8;
                        if (c<0x100) vidc.pal8[c]=makecol((r.r<<4)|r.r,(r.g<<4)|r.g,(r.b<<4)|r.b);
                }
        }
        vidcr[v>>26]=v;
        if ((v>>24)==0x80)
        {
                if (vidc.htot != ((v >> 14) & 0x3FF))
                {
                        vidc.htot = (v >> 14) & 0x3FF;
                        vidc_redovideotiming();
                }
                LOG_VIDC_REGISTERS("VIDC write htot = %d\n", vidc.htot);
        }
        if ((v>>24)==0xA0)
        {
                if (vidc.vtot != ((v >> 14) & 0x3FF) + 1)
                {
                        vidc.vtot = ((v >> 14) & 0x3FF) + 1;

                        if (vidc.vtot >= 350)
                                vidc.scanrate=1;
                        else
                                vidc.scanrate=0;
                }
                LOG_VIDC_REGISTERS("VIDC write vtot = %d\n", vidc.vtot);
        }
        if ((v>>24)==0x84)
        {
                if (vidc.sync != ((v >> 14) & 0x3FF))
                {
                        vidc.sync = (v >> 14) & 0x3FF;
                }
                LOG_VIDC_REGISTERS("VIDC write sync = %d\n", vidc.sync);
        }
        if ((v>>24)==0x88)
        {
                vidc.hbstart=(((v&0xFFFFFF)>>14)<<1)+1;
                LOG_VIDC_REGISTERS("VIDC write hbstart = %d\n", vidc.hbstart);
        }
        if ((v>>24)==0x8C)
        {
                vidc.hdstart2=((v&0xFFFFFF)>>14);
                recalcse();
                vidc_redovideotiming();                
                LOG_VIDC_REGISTERS("VIDC write hdstart2 = %d\n", vidc.hdstart2);
        }
        if ((v>>24)==0x90)
        {
                vidc.hdend2=((v&0xFFFFFF)>>14);
                recalcse();
                vidc_redovideotiming();
                LOG_VIDC_REGISTERS("VIDC write hdend2 = %d\n", vidc.hdend2);
        }
        if ((v>>24)==0x94) vidc.hbend=(((v&0xFFFFFF)>>14)<<1)+1;
        if ((v>>24)==0x98) { vidc.cx=((v&0xFFE000)>>13)+6; vidc.cxh=((v&0xFFF800)>>11)+24; }
        if ((v>>24)==0x9C) vidc.inter=(v&0xFFFFFF);
        if ((v>>24)==0xA4)
        {
                if (vidc.vsync != ((v >> 14) & 0x3FF) + 1)
                {
                        vidc.vsync = ((v >> 14) & 0x3FF) + 1;
                }
        }
        if ((v>>24)==0xA8)
        {
                vidc.vbstart=((v&0xFFFFFF)>>14)+1;
        }
        if ((v>>24)==0xAC)
        {
                vidc.vdstart=((v&0xFFFFFF)>>14)+1;
        }
        if ((v>>24)==0xB0)
        {
                vidc.vdend=((v&0xFFFFFF)>>14)+1;
        }
        if ((v>>24)==0xB4)
        {
                vidc.vbend=((v&0xFFFFFF)>>14)+1;
        }
        if ((v>>24)==0xB8) vidc.cys=(v&0xFFC000);
        if ((v>>24)==0xBC) vidc.cye=(v&0xFFC000);
        if ((v>>24)==0xC0)
        {
                soundhz = 250000 / ((v & 0xff) + 2);
                soundper = ((v & 0xff) + 2) << 10;
                soundper = (soundper * 24000) / vidc.clock;

                sound_set_period((v & 0xff) + 2);

                LOG_VIDC_REGISTERS("Sound frequency write %08X period %i\n",v,soundper);
        }
        if ((v>>24)==0xE0)
        {
                vidc.cr = v & 0xffffff;
                recalcse();
                vidc_redovideotiming();
                LOG_VIDC_REGISTERS("VIDC write ctrl %08X\n", vidc.cr);
        }
//        printf("VIDC write %08X\n",v);
}

void clearbitmap()
{
        clear(buffer);
}

void initvid()
{
        buffer = create_bitmap(2048, 2048);
        vidc.line = 0;
        vidc.clock = 24000;
}

void redopalette()
{
        int c;
        redrawpalette=1;
        for (c=0;c<0x14;c++)
        {
                writevidc(vidcr[c]);
        }
        redrawpalette=0;
}

void setredrawall()
{
        clear(buffer);
}

void closevideo()
{
}

void reinitvideo()
{
        setredrawall();
        redopalette();
}

int getvidcline()
{
        return vidc.line;
}
int getvidcwidth()
{
        return vidc.htot;
}

void archline(uint8_t *bp, int x1, int y, int x2, uint32_t col)
{
        int x;
        for (x=x1;x<=x2;x++)
                ((uint32_t *)bp)[x]=col;
}

static void vidc_poll(void *__p)
{
        int c;
        int mode;
//        int col=0;
        int x,xx;
        uint32_t temp;
        uint32_t *p;
        uint8_t *bp;
//        char s[256];
        int l=(vidc.line-16);
        int xoffset,xoffset2;
        int old_display_on = vidc.displayon;
        int vidc_cycles;
  
        vidc_cycles = vidcgetcycs();
        timer_advance_u64(&vidc.timer, (uint64_t)vidc_cycles << (32-10));

        if (!vidc.in_display)
        {
                vidc.in_display = 1;
                if (vidc.displayon)
                {
                        /*arm_dmacount = vidc.fetch_count;
                        arm_dmalatch = vidc.cycles_per_fetch;
                        arm_dmalength = 5;*/
                }
                return;
        }
        vidc.in_display = 0;
        if (!vidc.scanrate && !dblscan) l<<=1;
        if (vidc.scanrate) l+=32;
        if (vidc.scanrate && vidc.vtot>600) l-=40;
        if (palchange)
        {
                redolookup();
                palchange=0;
        }

        if (vidc.line==vidc.vbstart && !vidc.border_was_disabled)
        { 
                vidc.borderon=1; 
                flyback=0; 
                if (vidc.disp_y_min > l && vidc.displayon)
                        vidc.disp_y_min = l;
        }
        if (vidc.line==vidc.vdstart && !vidc.display_was_disabled)
        {
//                rpclog("VIDC addr %08X %08X\n",vinit,vidcr[VIDC_CR]);
                vidc.addr=vinit;
                vidc.caddr=cinit;
                vidc.displayon = vidc_displayon = 1;
                vidc.fetch_count = vidc.cycles_per_fetch;
                mem_dorefresh = 0;
                flyback=0; 
                if (vidc.disp_y_min > l && vidc.borderon)
                        vidc.disp_y_min = l;
        }
        if (vidc.line==vidc.vdend)
        {
                vidc.displayon = vidc_displayon = 0;
                mem_dorefresh = memc_refreshon && !vidc_displayon;
                ioc_irqa(IOC_IRQA_VBLANK);
                flyback=0x80;
                if (vidc.disp_y_max == -1)
                        vidc.disp_y_max = l;
                vidc.display_was_disabled = 1;
                LOG_VIDEO_FRAMES("Normal vsync; speed %d%%, ins=%d, inscount=%d, PC=%08X\n", inssec, ins, inscount, PC);
        }
        if (vidc.line==vidc.vbend) 
        { 
                vidc.borderon=0; 
                if (vidc.disp_y_max == -1)
                        vidc.disp_y_max = l;
                vidc.border_was_disabled = 1;
        }
        vidc.line++;
        LOG_VIDC_TIMING("++ vidc.line == %d\n", vidc.line);
        videodma=vidc.addr;
        mode=(vidcr[VIDC_CR]&0xF);
        if (hires) mode=2;

        if (l>=0 && vidc.line<=1023 && l<1536)
        {
                bp = (uint8_t *)buffer->line[l];
                if (!memc_videodma_enable)
                {
                        if (vidc.borderon)
                        {
                                int hb_start = vidc.hbstart, hb_end = vidc.hbend;
                                int hd_start = vidc.hdstart, hd_end = vidc.hdend;

                                if (!(vidcr[VIDC_CR] & 2))
                                {
                                        /*8MHz or 12MHz pixel rate*/
                                        hb_start *= 2;
                                        hb_end *= 2;
                                        hd_start *= 2;
                                        hd_end *= 2;
                                }

                                if (display_mode == DISPLAY_MODE_TV)
                                        archline(bp, TV_X_MIN, l, hb_start-1, 0);
                                if (vidc.hdend > vidc.hbend || !vidc.displayon)
                                        archline(bp, hb_start, l, hb_end-1, vidc.pal[16]);
                                else
                                {
                                        archline(bp, hb_start, l, hd_start-1, vidc.pal[16]);
                                        archline(bp, hd_start, l, hd_end-1, 0);
                                        archline(bp, hd_end, l, hb_end-1, vidc.pal[16]);
                                }
                                if (display_mode == DISPLAY_MODE_TV)
                                        archline(bp, hb_end, l, TV_X_MAX-1, 0);

                                if (l < vidc.y_min)
                                        vidc.y_min = l;
                                if ((l+1) > vidc.y_max)
                                        vidc.y_max = l+1;
                                if (l < vidc.disp_y_min)
                                        vidc.disp_y_min = l;
                                if ((l+1) > vidc.disp_y_max)
                                        vidc.disp_y_max = l+1;
                        }
                        else 
                                archline(bp, 0, l, 1023, 0);
                }
                else
                {
                        x=vidc.hbstart;
                        if (vidc.hdstart>x) x=vidc.hdstart;
                        xx=vidc.hbend;
                        if (vidc.hdend<xx) xx=vidc.hdend;
                        xoffset=xx-x;
                        if (!(vidcr[VIDC_CR]&2))
                        {
                                /*8MHz or 12MHz pixel rate*/
                                xoffset=200-(xoffset>>1);
                                if (vidc.hdstart<vidc.hbstart) xoffset2=xoffset+(vidc.hdstart-vidc.hbstart);
                                else                           xoffset2=xoffset;
                                xoffset<<=1;
                                xoffset2<<=1;
                        }
                        else
                        {
                                /*16MHz or 24MHz pixel rate*/
                                xoffset=400-(xoffset>>1);
                                if (vidc.hdstart<vidc.hbstart) xoffset2=xoffset+(vidc.hdstart-vidc.hbstart);
                                else                           xoffset2=xoffset;
                        }
                        if (hires) xoffset2=0;
                        if (vidc.displayon)
                        {
                                switch (mode)
                                {
                                        case 0: /*Mode 4: 320x256 8MHz 1bpp*/
                                        case 1: /*12MHz 1bpp*/
                                        for (x = vidc.hdstart*2; x < vidc.hdend*2; x += 32)
                                        {
                                                temp = ram[vidc.addr++];
                                                if (x < 2048)
                                                {
                                                        for (xx = 0; xx < 64; xx += 2)
                                                                ((uint32_t *)bp)[x+xx] = ((uint32_t *)bp)[x+xx+1] = vidc.pal[(temp>>xx)&1];
                                                }
                                                if (vidc.addr == vend + 4)
                                                        vidc.addr = vstart;
                                        }
                                        break;
                                        case 2: /*Mode 0: 640x256 16MHz 1bpp*/
                                        case 3: /*Mode 25: 640x480 24MHz 1bpp*/
                                        for (x = (hires ? vidc.hdstart*4 : vidc.hdstart); x < (hires ? vidc.hdend*4 : vidc.hdend); x += 32)
                                        {
                                                temp = ram[vidc.addr++];
                                                if (x < 2048)
                                                {
                                                        if (hires)
                                                        {
                                                                for (xx=0;xx<32;xx+=4)
                                                                {
                                                                        ((uint32_t *)bp)[x+xx]   = monolook[temp&0xF][0];
                                                                        ((uint32_t *)bp)[x+xx+1] = monolook[temp&0xF][1];
                                                                        ((uint32_t *)bp)[x+xx+2] = monolook[temp&0xF][2];
                                                                        ((uint32_t *)bp)[x+xx+3] = monolook[temp&0xF][3];
                                                                        temp>>=4;
                                                                }
                                                        }
                                                        else
                                                        {
                                                                for (xx=0;xx<32;xx++)
                                                                        ((uint32_t *)bp)[x+xx] = vidc.pal[(temp>>xx)&1];
                                                        }
//                                                        p += 32;
                                                }
                                                if (vidc.addr == vend + 4)
                                                        vidc.addr = vstart;
                                        }
                                        break;
                                        case 4: /*Mode 1: 320x256 8MHz 2bpp*/
                                        case 5: /*12MHz 2bpp*/
                                        for (x = vidc.hdstart*2; x < vidc.hdend*2; x += 32)
                                        {
                                                temp = ram[vidc.addr++];
                                                if (x < 2048)
                                                {
                                                        for (xx=0;xx<32;xx+=2)
                                                                ((uint32_t *)bp)[x+xx]=((uint32_t *)bp)[x+xx+1]=vidc.pal[(temp>>xx)&3];
                                                }
                                                if (vidc.addr == vend + 4)
                                                        vidc.addr = vstart;
                                        }
                                        break;
                                        case 6: /*Mode 8: 640x256 16MHz 2bpp*/
                                        case 7: /*Mode 26: 640x480 24MHz 2bpp*/
                                        for (x = vidc.hdstart; x < vidc.hdend; x += 16)
                                        {
                                                temp = ram[vidc.addr++];
                                                if (x < 2048)
                                                {
                                                        for (xx = 0; xx < 16; xx++)
                                                                ((uint32_t *)bp)[x+xx]=vidc.pal[temp>>(xx<<1)&3];
                                                        p+=16;
                                                }
                                                if (vidc.addr == vend + 4)
                                                        vidc.addr = vstart;
                                        }
                                        break;
                                        case 8: /*Mode 9: 320x256 8MHz 4bpp*/
                                        case 9: /*12MHz 4bpp*/
                                        for (x = vidc.hdstart*2; x < vidc.hdend*2; x += 16)
                                        {
                                                temp = ram[vidc.addr++];
                                                if (x < 2048)
                                                {
                                                        for (c = 0; c < 16; c += 2)
                                                                ((uint32_t *)bp)[x+c] = ((uint32_t *)bp)[x+c+1] = vidc.pal[(temp>>(c<<1))&0xF];
                                                }
                                                if (vidc.addr == vend + 4)
                                                        vidc.addr = vstart;
                                        }
                                        break;
                                        case 10: /*Mode 12: 640x256 16MHz 4bpp*/
                                        case 11: /*Mode 27: 640x480 24MHz 4bpp*/
                                        for (x = vidc.hdstart; x < vidc.hdend; x += 8)
                                        {
                                                temp = ram[vidc.addr++];
                                                if (x < 2048)
                                                {
                                                        for (c = 0; c < 8; c++)
                                                                ((uint32_t *)bp)[x+c]=vidc.pal[(temp>>(c<<2))&0xF];
                                                        p+=8;
                                                }
                                                if (vidc.addr==vend+4) vidc.addr=vstart;
                                        }
                                        break;
                                        case 12: /*Mode 13: 320x256 8bpp*/
                                        case 13: /*12MHz 8bpp*/
                                        for (x = vidc.hdstart*2; x < vidc.hdend*2; x += 8)
                                        {
                                                temp=ram[vidc.addr++];
                                                if (x < 2048)
                                                {
                                                        ((uint32_t *)bp)[x]=((uint32_t *)bp)[x+1]=vidc.pal8[temp&0xFF];
                                                        ((uint32_t *)bp)[x+2]=((uint32_t *)bp)[x+3]=vidc.pal8[(temp>>8)&0xFF];
                                                        ((uint32_t *)bp)[x+4]=((uint32_t *)bp)[x+5]=vidc.pal8[(temp>>16)&0xFF];
                                                        ((uint32_t *)bp)[x+6]=((uint32_t *)bp)[x+7]=vidc.pal8[(temp>>24)&0xFF];
                                                }
                                                if (vidc.addr==vend+4) vidc.addr=vstart;
                                        }
                                        break;
                                        case 14: /*Mode 15: 640x256 16MHz 8bpp*/
                                        case 15: /*Mode 28: 640x480 24MHz 8bpp*/
                                        for (x = vidc.hdstart; x < vidc.hdend; x += 4)
                                        {
                                                temp = ram[vidc.addr++];
                                                if (x < 2048)
                                                {
                                                        ((uint32_t *)bp)[x]   = vidc.pal8[temp&0xFF];
                                                        ((uint32_t *)bp)[x+1] = vidc.pal8[(temp>>8)&0xFF];
                                                        ((uint32_t *)bp)[x+2] = vidc.pal8[(temp>>16)&0xFF];
                                                        ((uint32_t *)bp)[x+3] = vidc.pal8[(temp>>24)&0xFF];
                                                }
                                                if (vidc.addr == vend + 4)
                                                        vidc.addr = vstart;
                                        }
                                        break;
                                }

                                switch (mode)
                                {
                                        case 0: /*Mode 4*/
                                        case 1:
                                        case 4: /*Mode 1*/
                                        case 5:
                                        case 8: /*Mode 9*/
                                        case 9:
                                        case 12: /*Mode 13*/
                                        case 13:
                                        if (vidc.hbstart<vidc.hdstart)
                                        {
                                                if (display_mode == DISPLAY_MODE_TV)
                                                	archline(bp, TV_X_MIN, l, (vidc.hdstart*2)-1, 0);
                                        	archline(bp, vidc.hbstart*2, l, (vidc.hdstart*2)-1, vidc.pal[0x10]);
                                        }
                                        else
                                        {
                                                if (display_mode == DISPLAY_MODE_TV)
                                                        archline(bp, TV_X_MIN, l, (vidc.hbstart*2)-1, 0);
                                        }
                                        if (vidc.hbend > vidc.hdend)
                                        {
                                                archline(bp, vidc.hdend*2, l, vidc.hbend*2, vidc.pal[0x10]);
                                                if (display_mode == DISPLAY_MODE_TV)
                                                        archline(bp, vidc.hbend*2, l, TV_X_MAX-1, 0);
                                        }
                                        else
                                        {
                                                if (display_mode == DISPLAY_MODE_TV)
                                                        archline(bp, vidc.hbend*2, l, TV_X_MAX-1, 0);
                                        }
                                        break;
                                        case 2:  /*Mode 0*/
                                        case 3:  /*Mode 25*/
                                        case 6:  /*Mode 8*/
                                        case 7:  /*Mode 26*/
                                        case 10: /*Mode 12*/
                                        case 11: /*Mode 27*/
                                        case 14: /*Mode 15*/
                                        case 15: /*Mode 28*/
                                        if (hires) break;
                                        if (vidc.hbstart < vidc.hdstart)
                                        {
                                                if (display_mode == DISPLAY_MODE_TV)
                                                	archline(bp, TV_X_MIN, l, vidc.hbstart-1, 0);
                                        	archline(bp, vidc.hbstart, l, vidc.hdstart-1, vidc.pal[0x10]);
                                        }
                                        else
                                        {
                                                if (display_mode == DISPLAY_MODE_TV)
                                                        archline(bp, TV_X_MIN, l, vidc.hbstart-1, 0);
                                        }
                                        if (vidc.hbend > vidc.hdend)
                                        {
                                                archline(bp, vidc.hdend, l, vidc.hbend, vidc.pal[0x10]);
                                                if (display_mode == DISPLAY_MODE_TV)
                                                        archline(bp, vidc.hbend, l, TV_X_MAX-1, 0);
                                        }
                                        else
                                        {
                                                if (display_mode == DISPLAY_MODE_TV)
                                                        archline(bp, vidc.hbend, l, TV_X_MAX-1, 0);
                                        }
                                        break;
                                }

                                if (((vidc.cys>>14)+2)<=vidc.line && ((vidc.cye>>14)+2)>vidc.line)
                                {
                                        if (hires)
                                        {
                                                x = (vidc.cx << 2) - 80;
                                                temp = ram[vidc.caddr++];
                                                for (xx = 0; xx < 64; xx += 4)
                                                {
                                                        if (temp & 3)
                                                                ((uint32_t *)bp)[x+xx]   = ((uint32_t *)bp)[x+xx+1] = 
                                                                ((uint32_t *)bp)[x+xx+2] = ((uint32_t *)bp)[x+xx+3] = hirescurcol[temp&3];
                                                        temp>>=2;
                                                }
                                                temp = ram[vidc.caddr++];
                                                for (xx = 64; xx < 128; xx += 4)
                                                {
                                                        if (temp & 3)
                                                                ((uint32_t *)bp)[x+xx]   = ((uint32_t *)bp)[x+xx+1] = 
                                                                ((uint32_t *)bp)[x+xx+2] = ((uint32_t *)bp)[x+xx+3] = hirescurcol[temp&3];
                                                        temp>>=2;
                                                }
                                        }
                                        else switch (vidcr[VIDC_CR]&0xF)
                                        {
                                                case 0: /*Mode 4*/
                                                case 1:
                                                case 4: /*Mode 1*/
                                                case 5:
                                                case 8: /*Mode 9*/
                                                case 9:
                                                case 12: /*Mode 13*/
                                                case 13:
                                                x = vidc.cx << 1;//((vidc.cx-vidc.hdstart)<<1)+xoffset2;
                                                if (x > (2048 - 32*2))
                                                        break;
                                                temp=ram[vidc.caddr++];
                                                for (xx=0;xx<32;xx+=2)
                                                {
                                                        if (temp&3) ((uint32_t *)bp)[x+xx]=((uint32_t *)bp)[x+xx+1]=vidc.pal[(temp&3)|0x10];
                                                        temp>>=2;
                                                }
                                                temp=ram[vidc.caddr++];
                                                for (xx=32;xx<64;xx+=2)
                                                {
                                                        if (temp&3) ((uint32_t *)bp)[x+xx]=((uint32_t *)bp)[x+xx+1]=vidc.pal[(temp&3)|0x10];
                                                        temp>>=2;
                                                }
                                                break;

                                                case 2:  /*Mode 0*/
                                                case 3:  /*Mode 25*/
                                                case 6:  /*Mode 8*/
                                                case 7:  /*Mode 26*/
                                                case 10: /*Mode 12*/
                                                case 11: /*Mode 27*/
                                                case 14: /*Mode 15*/
                                                case 15: /*Mode 28*/
                                                x = vidc.cx;//(vidc.cx-vidc.hdstart)+xoffset2;
                                                if (x > (2048-32))
                                                        break;
                                                temp=ram[vidc.caddr++];
                                                for (xx=0;xx<16;xx++)
                                                {
                                                        if (temp&3) ((uint32_t *)bp)[x+xx]=vidc.pal[(temp&3)|0x10];
                                                        temp>>=2;
                                                }
                                                temp=ram[vidc.caddr++];
                                                for (xx=16;xx<32;xx++)
                                                {
                                                        if (temp&3) ((uint32_t *)bp)[x+xx]=vidc.pal[(temp&3)|0x10];
                                                        temp>>=2;
                                                }
                                                break;
                                        }
                                }
                        }
                        if (vidc.borderon && !vidc.displayon)
                        {
                                int hb_start = vidc.hbstart, hb_end = vidc.hbend;
                                
                                if (!(vidcr[VIDC_CR] & 2))
                                {
                                        hb_start *= 2;
                                        hb_end *= 2;
                                }

                                if (display_mode == DISPLAY_MODE_TV)
                                        archline(bp, TV_X_MIN, l, hb_start-1, 0);
                                archline(bp, hb_start, l, hb_end-1, vidc.pal[16]);
                                if (display_mode == DISPLAY_MODE_TV)
                                        archline(bp, hb_end, l, TV_X_MAX-1, 0);
                        }
                        if (!vidc.borderon && vidc.displayon)
                                archline(bp,0,l,1023,0);
                        if (vidc.borderon && l < vidc.y_min)
                                vidc.y_min = l;
                        if (vidc.borderon && (l+1) > vidc.y_max)
                                vidc.y_max = l+1;
                }
        }
        else if (display_mode == DISPLAY_MODE_TV && l >= TV_Y_MIN && l < TV_Y_MAX)
                archline(buffer->line[l], TV_X_MIN, l, TV_X_MAX-1, 0);

        if (vidc.line>=vidc.vtot)
        {
                LOG_VIDEO_FRAMES("Frame over!  vidc.line=%d, vidc.vtot=%d\n", vidc.line, vidc.vtot);
                if (vidc.displayon)
                {
                        vidc.displayon = vidc_displayon = 0;
                        mem_dorefresh = memc_refreshon && !vidc_displayon;
                        ioc_irqa(IOC_IRQA_VBLANK);
                        flyback=0x80;
                        vidc.disp_y_max = l;
//                        rpclog("Late vsync\n");
                }

                oldflash=readflash[0]|readflash[1]|readflash[2]|readflash[3];

                if (display_mode == DISPLAY_MODE_NO_BORDERS || hires)
                {
                        int hd_start = (vidc.hbstart > vidc.hdstart) ? vidc.hbstart : vidc.hdstart;
                        int hd_end = (vidc.hbend < vidc.hdend) ? vidc.hbend : vidc.hdend;
                        int height = vidc.disp_y_max - vidc.disp_y_min;

                        if (hires)
                        {
                                hd_start = vidc.hdstart * 4;
                                hd_end = vidc.hdend * 4;
                        }
                        else if (!(vidcr[VIDC_CR] & 2))
                        {
                                hd_start *= 2;
                                hd_end *= 2;
                        }

                        if (vidc.scanrate || !dblscan)
                                updatewindowsize(hd_end - hd_start, height);
                        else
                                updatewindowsize(hd_end - hd_start, height * 2);

                        if (vidc.scanrate || !dblscan)
                        {
                                LOG_VIDEO_FRAMES("PRESENT: normal display\n");
                                updatewindowsize(hd_end-hd_start, height);
                                video_renderer_update(buffer, hd_start, vidc.disp_y_min, 0, 0, hd_end-hd_start, height);
                                video_renderer_present(0, 0, hd_end-hd_start, height, 0);
                        }
                        else
                        {
                                LOG_VIDEO_FRAMES("PRESENT: line doubled");
                                updatewindowsize(hd_end-hd_start, height * 2);
                                video_renderer_update(buffer, hd_start, vidc.disp_y_min, 0, 0, hd_end-hd_start, height);
                                video_renderer_present(0, 0, hd_end-hd_start, height, 1);
                        }
                }
                else if (display_mode == DISPLAY_MODE_NATIVE_BORDERS)
                {
                        LOG_VIDEO_FRAMES("BLIT: fullborders|fullscreen\n");
			int hb_start = vidc.hbstart;
			int hb_end = vidc.hbend;

                        if (!(vidcr[VIDC_CR] & 2))
                        {
                                hb_start *= 2;
                                hb_end *= 2;
                        }
				
                        if (vidc.scanrate || !dblscan)
                        {
                                LOG_VIDEO_FRAMES("UPDATE AND PRESENT: fullborders|fullscreen no doubling\n");
                                updatewindowsize(hb_end-hb_start, vidc.y_max-vidc.y_min);
                                video_renderer_update(buffer, hb_start, vidc.y_min, 0, 0, hb_end-hb_start, vidc.y_max-vidc.y_min);
                                video_renderer_present(0, 0, hb_end-hb_start, vidc.y_max-vidc.y_min, 0);
                        }
                        else
                        {
                                LOG_VIDEO_FRAMES("UPDATE AND PRESENT: fullborders|fullscreen + doubling\n");
                                updatewindowsize(hb_end-hb_start, (vidc.y_max-vidc.y_min) * 2);
                                video_renderer_update(buffer, hb_start, vidc.y_min, 0, 0, hb_end-hb_start, vidc.y_max-vidc.y_min);
                                video_renderer_present(0, 0, hb_end-hb_start, vidc.y_max-vidc.y_min, 1);
                        }
                }
                else
                {
                        LOG_VIDEO_FRAMES("BLIT: !(fullborders|fullscreen) dblscan=%d VIDC_CR=%08X\n", dblscan, vidcr[VIDC_CR]);
                        updatewindowsize(TV_X_MAX-TV_X_MIN, (TV_Y_MAX-TV_Y_MIN)*2);
                        if (vidcr[VIDC_CR] & 1)
                        {
                                if (dblscan)
                                {
                                        video_renderer_update(buffer, TV_X_MIN_24, TV_Y_MIN, 0, 0, TV_X_MAX_24-TV_X_MIN_24, TV_Y_MAX-TV_Y_MIN);
                                        video_renderer_present(0, 0, TV_X_MAX_24-TV_X_MIN_24, TV_Y_MAX-TV_Y_MIN, 1);
                                }
                                else
                                {
                                        video_renderer_update(buffer, TV_X_MIN_24, TV_Y_MIN*2, 0, 0, TV_X_MAX_24-TV_X_MIN_24, (TV_Y_MAX-TV_Y_MIN)*2);
                                        video_renderer_present(0, 0, TV_X_MAX_24-TV_X_MIN_24, (TV_Y_MAX-TV_Y_MIN)*2, 0);
                                }
                        }
                        else
                        {
                                if (dblscan)
                                {
                                        video_renderer_update(buffer, TV_X_MIN, TV_Y_MIN, 0, 0, TV_X_MAX-TV_X_MIN, TV_Y_MAX-TV_Y_MIN);
                                        video_renderer_present(0, 0, TV_X_MAX-TV_X_MIN, TV_Y_MAX-TV_Y_MIN, 1);
                                }
                                else
                                {
                                        video_renderer_update(buffer, TV_X_MIN, TV_Y_MIN*2, 0, 0, TV_X_MAX-TV_X_MIN, (TV_Y_MAX-TV_Y_MIN)*2);
                                        video_renderer_present(0, 0, TV_X_MAX-TV_X_MIN, (TV_Y_MAX-TV_Y_MIN)*2, 0);
                                }
                        }
                }

                vidc.y_min = 9999;
                vidc.y_max = 0;
                vidc.disp_y_min = 9999;
                vidc.disp_y_max = -1;
//                video_renderer_present();
/*                if (fullborders|fullscreen)
                {
                        if (readflash[0]) rectfill(bout,780,4,796,8,makecol(255,160,32));
                        if (readflash[1]) rectfill(bout,760,4,776,8,makecol(255,160,32));
                        if (readflash[2]) rectfill(bout,740,4,756,8,makecol(255,160,32));
                        if (readflash[3]) rectfill(bout,720,4,736,8,makecol(255,160,32));
                }
                else
                {
                        if (readflash[0]) rectfill(bout,652,4,668,8,makecol(255,160,32));
                        if (readflash[1]) rectfill(bout,632,4,648,8,makecol(255,160,32));
                        if (readflash[2]) rectfill(bout,612,4,628,8,makecol(255,160,32));
                        if (readflash[3]) rectfill(bout,592,4,608,8,makecol(255,160,32));
                }
                readflash[0]=readflash[1]=readflash[2]=readflash[3]=0;*/
//                LOG_VIDEO_FRAMES("Blit\n");

                /*Clear the buffer now so we don't get a persistent ghost when changing
                  from a high vertical res mode to a line-doubled mode.*/
                clear(buffer);

                vidc.line=0;
                vidc.border_was_disabled = 0;
                vidc.display_was_disabled = 0;
//                rpclog("%i fetches\n", vidc_fetches);
                vidc_fetches = 0;
                vidc_framecount++;
        }
        
        if (!vidc.displayon && !old_display_on)
        {
                if (memc_refreshon)
                {
                        /*arm_dmacount  = 32 << 10;
                        arm_dmalatch  = 32 << 10;
                        arm_dmalength =  2;*/
                }
                else
                {
                        /*arm_dmacount = 0x7fffffff;
                        arm_dmalatch = 0x7fffffff;
                        arm_dmalength = 0;*/
                }
        }
        else if (vidc.displayon)
        {
                vidc.fetch_count = arm_dmacount;
                /*arm_dmacount = 0x7fffffff;
                arm_dmalatch = 0x7fffffff;
                arm_dmalength = 0;*/
        }
}

void vidc_redovideotiming()
{
        vidc.cyclesperline_display = vidc.cyclesperline_blanking = 0;
}

/*Return the number of clock ticks * 1024 in the next display line*/
int vidcgetcycs()
{
        if (!vidc.cyclesperline_display)
        {
                int temp, temp2;
                int displen = vidc.hdend2 - vidc.hdstart2;
                int disp_rate;
                
                if (displen < 0)
                        displen = 0;

                temp  = displen * 4;
                temp2 = ((vidc.htot - displen) + 1) * 4;
                rpclog("displen %i, htot %d\n", displen, vidc.htot);
                switch (vidcr[VIDC_CR]&3)
                {
                        case 0: /*8MHz pixel rate*/
			disp_rate = displen / 64;
                        break;
                        case 1: /*12MHz pixel rate*/
                        temp  = (temp * 2) / 3; 
                        temp2 = (temp2 * 2) / 3;
                        disp_rate = displen / 32;
                        break;
                        case 2: /*16MHz pixel rate*/
                        temp  = temp / 2; 
                        temp2 = temp2 / 2;
                        disp_rate = displen / 16;
                        break;
                        case 3: /*24MHz pixel rate*/
                        temp  = temp / 3; 
                        temp2 = temp2 / 3;
                        disp_rate = displen / 8;
                        break;
                }

		switch (vidcr[VIDC_CR] & 0xc)
                {
                        case 0x0: /*1bpp*/
			disp_rate = displen / 64;
                        break;
                        case 0x4: /*2bpp*/
                        disp_rate = displen / 32;
                        break;
                        case 0x8: /*4bpp*/
                        disp_rate = displen / 16;
                        break;
                        case 0xc: /*8bpp*/
                        disp_rate = displen / 8;
                        break;
                }
		if (disp_rate)
			disp_rate = displen / disp_rate;
		rpclog("disp_rate %i\n", disp_rate);
		vidc.disp_len = displen;
		vidc.disp_rate = disp_rate;
		
                vidc.cyclesperline_display  = (((temp * speed_mhz) / 8) / 2) << 10;
                vidc.cyclesperline_blanking = (((temp2 * speed_mhz) / 8) / 2) << 10;
                rpclog("set cyclesperline display=%d blanking=%d\n",
                        vidc.cyclesperline_display, vidc.cyclesperline_blanking);
        
                if (!vidc.cyclesperline_display)
                        vidc.cyclesperline_display = 512 << 10;
                if (!vidc.cyclesperline_blanking)
                        vidc.cyclesperline_blanking = 512 << 10;
                rpclog("2 cyclesperline display=%d blanking=%d\n",
                        vidc.cyclesperline_display, vidc.cyclesperline_blanking);
        
                vidc.cyclesperline_display  = (int32_t)(((int64_t)vidc.cyclesperline_display  * 24000) / vidc.clock);
                vidc.cyclesperline_blanking = (int32_t)(((int64_t)vidc.cyclesperline_blanking * 24000) / vidc.clock);
                // rpclog("cyclesperline = %i %i  %i %i %i  %i\n", vidc.cyclesperline_display, vidc.cyclesperline_blanking, vidc.htot, vidc.hdend2, vidc.hdstart2, speed_mhz);
                rpclog("3 cyclesperline display=%d blanking=%d\n",
                        vidc.cyclesperline_display, vidc.cyclesperline_blanking);

                temp = (128 >> ((vidc.cr >> 2) & 3)) << 10; /*Pixel clocks per fetch*/
                rpclog("pixel clocks per fetch %i %i\n", temp, temp >> 10);
                switch (vidcr[VIDC_CR]&3)
                {
                        case 0: /*8MHz pixel rate*/
                        break;
                        case 1: /*12MHz pixel rate*/
                        temp  = (temp * 2) / 3; 
                        break;
                        case 2: /*16MHz pixel rate*/
                        temp  = temp / 2; 
                        break;
                        case 3: /*24MHz pixel rate*/
                        temp  = temp / 3; 
                        break;
                }       /*Bus clocks per fetch*/

                temp = (int32_t)(((int64_t)temp * 24000) / vidc.clock);
                vidc.cycles_per_fetch = (temp * speed_mhz) / 8;
                rpclog("cycles_per_fetch %i\n", vidc.cycles_per_fetch);
        }

        if (vidc.in_display)
        {
                if (vidc.cyclesperline_display < 0)
                {
                        rpclog("vidc.cyclesperline_display == %d, should be positive.  clock=%d\n",
                                vidc.cyclesperline_display, vidc.clock);
                        return 512 << 10;
                }
                return vidc.cyclesperline_display;
        }

        if (vidc.cyclesperline_blanking < 0)
        {
                rpclog("vidc.cyclesperline_blanking == %d, should be positive.  clock=%d\n",
                        vidc.cyclesperline_blanking, vidc.clock);
                return 512 << 10;
        }

        return vidc.cyclesperline_blanking;
}

int vidc_update_cycles()
{
//rpclog("Fetch at %i %i  %i,%i  %i %i %i,%i,%i\n", vidc.in_display, vidc_fetches, (vidc.cyclesperline_display-cycles) >> 10, vidc.line, time,cycles, vidc.disp_count, vidc.disp_rate, vidc.disp_len);
	if (!vidc.displayon)
	{
		vidc_dma_length = memc_refreshon ? mem_speed[0][1] : 0;
//		rpclog("!displayon %i %i\n", vidc.cyclesperline_display + vidc.cyclesperline_blanking, cycles);
		return memc_refresh_time;
	}
	if (vidc.in_display && vidc.disp_count < vidc.disp_len)
	{
		if (memc_videodma_enable)
			vidc_dma_length = mem_speed[0][1] + mem_speed[0][0]*3;
		else
			vidc_dma_length = 0;
		vidc.disp_count += vidc.disp_rate;
		return vidc.cycles_per_fetch;
	}
	else
	{
		vidc_dma_length = 0;
		if (!vidc.in_display)
			vidc.disp_count = 0;
		return 10000;
	}
}

void vidc_setclock(int clock)
{
        switch (clock)
        {
                case 0:
                vidc.clock = 24000;
                break;
                case 1:
                vidc.clock = 25175;
                break;
                case 2:
                vidc.clock = 36000;
                break;
        }
        sound_set_clock((vidc.clock * 1000) / 24);
}

int vidc_getclock()
{
        return vidc.clock;
}

void vidc_reset()
{
        timer_add(&vidc.timer, vidc_poll, NULL, 1);
        vidc_setclock(0);
        sound_set_period(255);
}
