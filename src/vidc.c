/*Arculator 0.8 by Tom Walker
  VIDC10 emulation*/
#include <stdio.h>
#if WIN32
#define BITMAP __win_BITMAP
#include <windows.h>
#undef BITMAP
#endif
#include "arc.h"
#include "arm.h"
#include "ioc.h"
#include "mem.h"
#include "memc.h"
#include "sound.h"
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

BITMAP *create_bitmap(int x, int y)
{
        BITMAP *b = malloc(sizeof(BITMAP) + (y * sizeof(uint8_t *)));
        int c;
        b->dat = malloc(x * y * 4);
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
extern int cycles;
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
        switch (vidcr[0x38]&0xF)
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
        int c,d,oldscanrate=vidc.scanrate,oldvtot=vidc.vtot;
//        rpclog("Write VIDC %08X\n",v);
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
//                rpclog("Write pal %08X %08X %08X %i\n",c,vidc.pal[(v>>26)&0x1F],v,get_color_depth());
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
        }
        if ((v>>24)==0x84)
        {
                if (vidc.sync != ((v >> 14) & 0x3FF))
                {
                        vidc.sync = (v >> 14) & 0x3FF;
                }
        }
        if ((v>>24)==0x88) vidc.hbstart=(((v&0xFFFFFF)>>14)<<1)+1;
        if ((v>>24)==0x8C)
        {
                vidc.hdstart2=((v&0xFFFFFF)>>14);
                recalcse();
                vidc_redovideotiming();                
        }
        if ((v>>24)==0x90)
        {
                vidc.hdend2=((v&0xFFFFFF)>>14);
                recalcse();
                vidc_redovideotiming();
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
//                rpclog("Sound frequency write %08X period %i\n",v,soundper);
        }
        if ((v>>24)==0xE0)
        {
                vidc.cr = v & 0xffffff;
                recalcse();
                vidc_redovideotiming();
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

void pollline()
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

        if (vidc.line==vidc.vbstart) 
        { 
                vidc.borderon=1; 
                flyback=0; 
                if (vidc.disp_y_min > l && vidc.displayon)
                        vidc.disp_y_min = l;
        }
        if (vidc.line==vidc.vdstart)
        {
//                rpclog("VIDC addr %08X %08X\n",vinit,vidcr[0x38]);
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
//                rpclog("Normal vsync\n");
        }
        if (vidc.line==vidc.vbend) 
        { 
                vidc.borderon=0; 
                if (vidc.disp_y_max == -1)
                        vidc.disp_y_max = l;
        }
        vidc.line++;
        videodma=vidc.addr;
        mode=(vidcr[0x38]&0xF);
        if (hires) mode=2;

        if (l>=0 && vidc.line<=1023 && l<1536)
        {
                bp = (uint8_t *)buffer->line[l];
                if (!memc_videodma_enable)
                {
                        archline(bp, 0, l, 1023, 0);
                }
                else
                {
                        x=vidc.hbstart;
                        if (vidc.hdstart>x) x=vidc.hdstart;
                        xx=vidc.hbend;
                        if (vidc.hdend<xx) xx=vidc.hdend;
                        xoffset=xx-x;
                        if (!(vidcr[0x38]&2))
                        {
                                xoffset=200-(xoffset>>1);
                                if (vidc.hdstart<vidc.hbstart) xoffset2=xoffset+(vidc.hdstart-vidc.hbstart);
                                else                           xoffset2=xoffset;
                                xoffset<<=1;
                                xoffset2<<=1;
                        }
                        else
                        {
                                xoffset=400-(xoffset>>1);
                                if (vidc.hdstart<vidc.hbstart) xoffset2=xoffset+(vidc.hdstart-vidc.hbstart);
                                else                           xoffset2=xoffset;
                        }
                        if (hires) xoffset2=0;
                        if (vidc.displayon)
                        {
                                switch (mode)
                                {
                                        case 0: /*Mode 4*/
                                        case 1:
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
                                        case 2: /*Mode 0*/
                                        case 3: /*Mode 25*/
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
                                        case 4: /*Mode 1*/
                                        case 5:
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
                                        case 6: /*Mode 8*/
                                        case 7: /*Mode 26*/
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
                                        case 8: /*Mode 9*/
                                        case 9:
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
                                        case 10: /*Mode 12*/
                                        case 11: /*Mode 27*/
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
                                        case 12: /*Mode 13*/
                                        case 13:
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
                                        case 14: /*Mode 15*/
                                        case 15: /*Mode 28*/
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
                                        else switch (vidcr[0x38]&0xF)
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
                                
                                if (!(vidcr[0x38] & 2))
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
                archline(bp, TV_X_MIN, l, TV_X_MAX-1, 0);

        if (vidc.line>=vidc.vtot)
        {
//                rpclog("Frame over!\n");
                if (vidc.displayon)
                {
                        vidc.displayon = vidc_displayon = 0;
                        mem_dorefresh = memc_refreshon && !vidc_displayon;
                        ioc_irqa(IOC_IRQA_VBLANK);
                        flyback=0x80;
                        vidc.disp_y_max = l;
//                        rpclog("Late vsync\n");
                }
                blitcount++;
                if (!(blitcount&7) || limitspeed)
                {
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
                                else if (!(vidcr[0x38] & 2))
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
                                        updatewindowsize(hd_end-hd_start, height);
                                        video_renderer_update(buffer, hd_start, vidc.disp_y_min, 0, 0, hd_end-hd_start, height);
                                        video_renderer_present(0, 0, hd_end-hd_start, height);
                                }
                                else
                                {
                                        updatewindowsize(hd_end-hd_start, height * 2);
                                        video_renderer_update(buffer, hd_start, vidc.disp_y_min, 0, 0, hd_end-hd_start, height);
                                        video_renderer_present(0, 0, hd_end-hd_start, height);
                                }
                        }
                        else if (display_mode == DISPLAY_MODE_NATIVE_BORDERS)
                        {
				int blit_x_offset;
				int hb_start = vidc.hbstart;
				int hb_end = vidc.hbend;

                                if (!(vidcr[0x38] & 2))
                                {
                                        hb_start *= 2;
                                        hb_end *= 2;
                                }
				
                                if (vidc.scanrate || !dblscan)
                                {
                                        updatewindowsize(hb_end-hb_start, vidc.y_max-vidc.y_min);
                                        video_renderer_update(buffer, hb_start, vidc.y_min, 0, 0, hb_end-hb_start, vidc.y_max-vidc.y_min);
                                        video_renderer_present(0, 0, hb_end-hb_start, vidc.y_max-vidc.y_min);
                                }
                                else
                                {
                                        updatewindowsize(hb_end-hb_start, (vidc.y_max-vidc.y_min) * 2);
                                        video_renderer_update(buffer, hb_start, vidc.y_min, 0, 0, hb_end-hb_start, vidc.y_max-vidc.y_min);
                                        video_renderer_present(0, 0, hb_end-hb_start, vidc.y_max-vidc.y_min);
                                }
                        }
                        else
                        {
                                updatewindowsize(TV_X_MAX-TV_X_MIN, (TV_Y_MAX-TV_Y_MIN)*2);
                                if (vidcr[0x38] & 1)
                                {
                                        if (dblscan)
                                        {
                                                video_renderer_update(buffer, TV_X_MIN_24, TV_Y_MIN, 0, 0, TV_X_MAX_24-TV_X_MIN_24, TV_Y_MAX-TV_Y_MIN);
                                                video_renderer_present(0, 0, TV_X_MAX_24-TV_X_MIN_24, TV_Y_MAX-TV_Y_MIN);
                                        }
                                        else
                                        {
                                                video_renderer_update(buffer, TV_X_MIN_24, TV_Y_MIN*2, 0, 0, TV_X_MAX_24-TV_X_MIN_24, (TV_Y_MAX-TV_Y_MIN)*2);
                                                video_renderer_present(0, 0, TV_X_MAX_24-TV_X_MIN_24, (TV_Y_MAX-TV_Y_MIN)*2);
                                        }
                                }
                                else
                                {
                                        if (dblscan)
                                        {
                                                video_renderer_update(buffer, TV_X_MIN, TV_Y_MIN, 0, 0, TV_X_MAX-TV_X_MIN, TV_Y_MAX-TV_Y_MIN);
                                                video_renderer_present(0, 0, TV_X_MAX-TV_X_MIN, TV_Y_MAX-TV_Y_MIN);
                                        }
                                        else
                                        {
                                                video_renderer_update(buffer, TV_X_MIN, TV_Y_MIN*2, 0, 0, TV_X_MAX-TV_X_MIN, (TV_Y_MAX-TV_Y_MIN)*2);
                                                video_renderer_present(0, 0, TV_X_MAX-TV_X_MIN, (TV_Y_MAX-TV_Y_MIN)*2);
                                        }
                                }
                        }

                        vidc.y_min = 9999;
                        vidc.y_max = 0;
                        vidc.disp_y_min = 9999;
                        vidc.disp_y_max = -1;
//                        video_renderer_present();
/*                        if (fullborders|fullscreen)
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
                        rpclog("Blit\n");
                }
                vidc.line=0;
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
rpclog("displen %i\n", displen);
                switch (vidcr[0x38]&3)
                {
                        case 0:
			disp_rate = displen / 64;
                        break;
                        case 1: 
                        temp  = (temp * 2) / 3; 
                        temp2 = (temp2 * 2) / 3;
                        disp_rate = displen / 32;
                        break;
                        case 2: 
                        temp  = temp / 2; 
                        temp2 = temp2 / 2;
                        disp_rate = displen / 16;
                        break;
                        case 3: 
                        temp  = temp / 3; 
                        temp2 = temp2 / 3;
                        disp_rate = displen / 8;
                        break;
                }

		switch (vidcr[0x38] & 0xc)
                {
                        case 0x0:
			disp_rate = displen / 64;
                        break;
                        case 0x4: 
                        disp_rate = displen / 32;
                        break;
                        case 0x8:
                        disp_rate = displen / 16;
                        break;
                        case 0xc:
                        disp_rate = displen / 8;
                        break;
                }
rpclog("disp_rate %i\n", disp_rate);
		if (disp_rate)
			disp_rate = displen / disp_rate;
		rpclog("disp_rate %i\n", disp_rate);
		vidc.disp_len = displen;
		vidc.disp_rate = disp_rate;
		
                vidc.cyclesperline_display  = (((temp * speed_mhz) / 8) / 2) << 10;
                vidc.cyclesperline_blanking = (((temp2 * speed_mhz) / 8) / 2) << 10;
        
                if (!vidc.cyclesperline_display)
                        vidc.cyclesperline_display = 512 << 10;
                if (!vidc.cyclesperline_blanking)
                        vidc.cyclesperline_blanking = 512 << 10;
        
                vidc.cyclesperline_display  = (int32_t)(((int64_t)vidc.cyclesperline_display  * 24000) / vidc.clock);
                vidc.cyclesperline_blanking = (int32_t)(((int64_t)vidc.cyclesperline_blanking * 24000) / vidc.clock);
                rpclog("cyclesperline = %i %i  %i %i %i  %i\n", vidc.cyclesperline_display, vidc.cyclesperline_blanking, vidc.htot, vidc.hdend2, vidc.hdstart2, speed_mhz);

                temp = (128 >> ((vidc.cr >> 2) & 3)) << 10; /*Pixel clocks per fetch*/
                rpclog("pixel clocks per fetch %i %i\n", temp, temp >> 10);
                switch (vidcr[0x38]&3)
                {
                        case 0: 
                        break;
                        case 1: 
                        temp  = (temp * 2) / 3; 
                        break;
                        case 2: 
                        temp  = temp / 2; 
                        break;
                        case 3: 
                        temp  = temp / 3; 
                        break;
                }       /*Bus clocks per fetch*/

                temp = (int32_t)(((int64_t)temp * 24000) / vidc.clock);
                vidc.cycles_per_fetch = (temp * speed_mhz) / 8;
                rpclog("cycles_per_fetch %i\n", vidc.cycles_per_fetch);
        }

        if (vidc.in_display)
                return vidc.cyclesperline_display;

        return vidc.cyclesperline_blanking;
}

int vidc_update_cycles()
{
//rpclog("Fetch at %i %i  %i,%i  %i %i %i,%i,%i\n", vidc.in_display, vidc_fetches, (vidc.cyclesperline_display-cycles) >> 10, vidc.line, time,cycles, vidc.disp_count, vidc.disp_rate, vidc.disp_len);
	if (!vidc.displayon)
	{
		vidc_dma_length = memc_refreshon ? mem_speed[0][1] : 0;
//		rpclog("!displayon %i %i\n", vidc.cyclesperline_display + vidc.cyclesperline_blanking, cycles);
		return memc_refresh_time;//cycles;
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
		return cycles;
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
}

int vidc_getclock()
{
        return vidc.clock;
}
