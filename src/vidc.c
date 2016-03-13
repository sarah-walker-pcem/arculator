/*Arculator 0.8 by Tom Walker
  VIDC10 emulation*/
#include <stdio.h>
#include <allegro.h>
#ifdef WIN32
#include <winalleg.h>
#endif
#include "arc.h"
#include "arm.h"
#include "ioc.h"
#include "mem.h"
#include "memc.h"
#include "sound.h"
#include "vidc.h"

int vidc_dma_length;
extern int vidc_fetches;
extern int cycles;
int vidc_framecount = 0;
int vidc_displayon = 0;
int blitcount=0;
/*b - memory buffer
  vbuf - DirectX buffer (used for hardware blitting)*/
BITMAP *buffer, *vbuf;

int redrawall;
int flyback;
int deskdepth;
int videodma=0;
int palchange;
uint32_t vidlookup[256];   /*Lookup table for 4bpp modes*/

int redrawpalette=0;

int oldxxh,oldxxl,oldyh,oldyl;

int yh,yl,xxh,xxl;

int oldflash;

int startb,endb;


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
        
        int resync;
        
        int x_min, x_max, x_mid;
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
        if ((v>>26)==0x38 && v!=vidcr[0x38]) redrawall=(fullscreen)?4:2;
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
                if (((v>>26)&0x1F)==0x10 && c!=vidc.pal[0x10])
                {
                        redrawall=(fullscreen)?4:2;
//                        rpclog("Border change\n");
                }
                d=v>>26;
                palchange=1;
//                if ((v>>26)==0x10)
//                   redrawall=2;
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
                        vidc.resync = 2;
                        vidc.htot = (v >> 14) & 0x3FF;
                        vidc_redovideotiming();
                }
        }
        if ((v>>24)==0xA0)
        {
                if (vidc.vtot != ((v >> 14) & 0x3FF) + 1)
                {
                        vidc.resync = 2;
                        vidc.vtot = ((v >> 14) & 0x3FF) + 1;

                        if (vidc.vtot>400)
                                vidc.scanrate=1;
                        else
                                vidc.scanrate=0;
                        if ((vidc.scanrate!=oldscanrate) || (vidc.vtot!=oldvtot))
                        {
                                clear(screen);
                                clear(vbuf);
//                        clear(b);
                                redrawall=(fullscreen)?4:2;
                        }
                }
        }
        if ((v>>24)==0x84)
        {
                if (vidc.sync != ((v >> 14) & 0x3FF))
                {
                        vidc.resync = 2;
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
                        vidc.resync = 2;
                        vidc.vsync = ((v >> 14) & 0x3FF) + 1;
                }
        }
        if ((v>>24)==0xA8)
        {
                vidc.vbstart=((v&0xFFFFFF)>>14)+1;
                redrawall=(fullscreen)?4:2;
        }
        if ((v>>24)==0xAC)
        {
                vidc.vdstart=((v&0xFFFFFF)>>14)+1;
                redrawall=(fullscreen)?4:2;
        }
        if ((v>>24)==0xB0)
        {
                vidc.vdend=((v&0xFFFFFF)>>14)+1;
                redrawall=(fullscreen)?4:2;
        }
        if ((v>>24)==0xB4)
        {
                vidc.vbend=((v&0xFFFFFF)>>14)+1;
                redrawall=(fullscreen)?4:2;
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
        redrawall=(fullscreen)?4:2;
//        clear(b);
        clear(vbuf);
}

void initvid()
{
        int depth;
        allegro_init();
        depth=deskdepth=desktop_color_depth();
        if (depth!=15 && depth!=16 && depth!=32)
        {
                error("Your desktop must be set to 16 or 32 bit colour!");
                exit(-1);
        }
        #ifdef WIN32
        set_color_depth(depth);
        set_gfx_mode(GFX_AUTODETECT_WINDOWED,1152,1152,0,0);
        vbuf = create_video_bitmap(2048, 2048);
        #else
        if (depth==16 || depth==15)
        {
                set_color_depth(16);
                if (set_gfx_mode(GFX_AUTODETECT_WINDOWED,800,600,0,0))
                {
                        set_color_depth(15);
                        depth=15;
                        set_gfx_mode(GFX_AUTODETECT_WINDOWED,800,600,0,0);
                }
        }
        else if (depth==32)
        {
                set_color_depth(32);
                set_gfx_mode(GFX_AUTODETECT_WINDOWED,800,600,0,0);
        }
        vbuf = create_video_bitmap(2048, 2048);
        #endif
        set_color_depth(32);
        buffer = create_bitmap(2048, 2048);
        vidc.line = 0;
        vidc.clock = 24000;
        clear(vbuf);
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
        oldxxh=oldxxl=oldyh=oldyl=-1;
                redrawall=(fullscreen)?4:2;
//        clear(b);
        clear(vbuf);
}

void closevideo()
{
        rpclog("destroy_bitmap(vbuf)\n");
        destroy_bitmap(vbuf);
        rpclog("destroy_bitmap(vbuf) done\n");
//        destroy_bitmap(b);
//        allegro_exit();
}

void reinitvideo()
{
        destroy_bitmap(vbuf);
#ifdef WIN32
        if (fullscreen)
        {
                set_color_depth(32);
                if (hires)
                        set_gfx_mode(GFX_AUTODETECT_FULLSCREEN,1152,896,0,0);
                else
                        set_gfx_mode(GFX_AUTODETECT_FULLSCREEN,800,600,0,0);
        }
        else
        {
                set_color_depth(deskdepth);
                set_gfx_mode(GFX_AUTODETECT_WINDOWED,1152,1152,0,0);
        }
        vbuf = create_video_bitmap(2048, 2048);
#else
        set_gfx_mode(GFX_AUTODETECT_WINDOWED,800,600,0,0);
        vbuf = create_video_bitmap(2048, 2048);
#endif
        set_color_depth(32);
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
        BITMAP *bout=screen;
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
                vidc.borderon=0; 
                flyback=0; 
                /*rpclog("border off %i %i\n",vidc.line,l);*/ 
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
                if (yl==-1)
                        yl=l;
        }
        if (vidc.line==vidc.vdend)
        {
                vidc.displayon = vidc_displayon = 0;
                mem_dorefresh = memc_refreshon && !vidc_displayon;
                ioc_irqa(IOC_IRQA_VBLANK);
                flyback=0x80;
                if (yh==-1) yh=l;
//                rpclog("Normal vsync\n");
        }
        if (vidc.line==vidc.vbend) 
        { 
                vidc.borderon=1; 
                if (yh==-1) 
                        yh=l; 
                /*rpclog("Border on %i %i\n",vidc.line,l);*/ 
        }
        vidc.line++;
        videodma=vidc.addr;
        mode=(vidcr[0x38]&0xF);
        if (hires) mode=2;

        if (l>=0 && vidc.line<=((hires)?1023:((vidc.scanrate)?800:316)) && l<1536)
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
                                int x_min, x_max;

                                switch (mode)
                                {
                                        case 0: /*Mode 4*/
                                        case 1:
                                        if (vidc.hdstart*2 < vidc.x_min)
                                                vidc.x_min = vidc.hdstart*2;
                                        if (vidc.hdend*2 > vidc.x_max)
                                                vidc.x_max = vidc.hdend*2;
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
                                        if ((hires ? vidc.hdstart*4 : vidc.hdstart) < vidc.x_min)
                                                vidc.x_min = hires ? vidc.hdstart*4 : vidc.hdstart;
                                        if ((hires ? vidc.hdend*4 : vidc.hdend) > vidc.x_max)
                                                vidc.x_max = hires ? vidc.hdend*4 : vidc.hdend;
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
                                        if (vidc.hdstart*2 < vidc.x_min)
                                                vidc.x_min = vidc.hdstart*2;
                                        if (vidc.hdend*2 > vidc.x_max)
                                                vidc.x_max = vidc.hdend*2;
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
                                        if (vidc.hdstart < vidc.x_min)
                                                vidc.x_min = vidc.hdstart;
                                        if (vidc.hdend > vidc.x_max)
                                                vidc.x_max = vidc.hdend;
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
                                        x_min = (vidc.hdstart < vidc.hbstart) ? vidc.hbstart*2 : vidc.hdstart*2;
                                        x_max = (vidc.hdend   > vidc.hbend)   ? vidc.hbend*2 : vidc.hdend*2;
                                        
                                        if (x_min < vidc.x_min)
                                                vidc.x_min = x_min;
                                        if (x_max > vidc.x_max)
                                                vidc.x_max = x_max;
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
                                        if (vidc.hdstart < vidc.x_min)
                                                vidc.x_min = vidc.hdstart;
                                        if (vidc.hdend > vidc.x_max)
                                                vidc.x_max = vidc.hdend;
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
                                        x_min = (vidc.hdstart < vidc.hbstart) ? vidc.hbstart*2 : vidc.hdstart*2;
                                        x_max = (vidc.hdend   > vidc.hbend)   ? vidc.hbend*2 : vidc.hdend*2;
                                        
                                        if (x_min < vidc.x_min)
                                                vidc.x_min = x_min;
                                        if (x_max > vidc.x_max)
                                                vidc.x_max = x_max;
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
                                        if (vidc.hdstart < vidc.x_min)
                                                vidc.x_min = vidc.hdstart;
                                        if (vidc.hdend > vidc.x_max)
                                                vidc.x_max = vidc.hdend;
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
//#if 0
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
                                        	archline(bp,0,l,(vidc.hdstart<<1)-1,vidc.pal[0x10]);
                                        else
                                        {
                                                if (noborders && !fullscreen) xxh=xoffset-1;
                                                archline(bp,0,l,(vidc.hbstart<<1)-1,vidc.pal[0x10]);
                                        }
                                        if (1)
                                        {
                                                if (noborders && !fullscreen) xxl=xoffset+((vidc.hbend-vidc.hbstart)<<1);
                                                if (vidc.hbend < vidc.hdend)
                                                	archline(bp,vidc.hbend<<1,l,1023,vidc.pal[0x10]);
                                                else
                                                	archline(bp,vidc.hdend<<1,l,1023,vidc.pal[0x10]);
                                        }
                                        else
                                           archline(bp,0,l,1023,vidc.pal[0x10]);
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
                                        if (vidc.hbstart<vidc.hdstart)
                                        	archline(bp,0,l,vidc.hdstart-1,vidc.pal[0x10]);
                                        else
                                        {
                                                if (noborders && !fullscreen) xxh=xoffset-1;
                                                archline(bp,0,l,vidc.hdstart-1,vidc.pal[0x10]);
                                        }
                                        if (vidc.hbend<1024)
                                        {
                                                if (noborders && !fullscreen) xxl=xoffset+(vidc.hbend-vidc.hbstart);
                                                if (vidc.hbend < vidc.hdend)
                                                	archline(bp,vidc.hbend,l,1023,vidc.pal[0x10]);
                                                else
                                                	archline(bp,vidc.hdend,l,1023,vidc.pal[0x10]);
                                        }
                                        else
                                           archline(bp,0,l,1023,vidc.pal[0x10]);
                                        break;
                                }
//#endif
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
                                                if (x>1024) break;
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
                                                if (x>1024) break;
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
                        if ((vidc.borderon || !vidc.displayon))// && redrawall && !((noborders || hires) && !fullscreen))
                        {
                                archline(bp,0,l,1023,vidc.pal[16]);
                        }
                }
        }

        if (vidc.line>=vidc.vtot)
        {
                if (vidc.resync)
                {
                        vidc.resync--;
                        if (!vidc.resync)
                                vidc.x_mid = (vidc.x_min + vidc.x_max) / 2;
                }
//                rpclog("Frame over!\n");
                if (vidc.displayon)
                {
                        vidc.displayon = vidc_displayon = 0;
                        mem_dorefresh = memc_refreshon && !vidc_displayon;
                        ioc_irqa(IOC_IRQA_VBLANK);
                        flyback=0x80;
                        yh=l;
//                        rpclog("Late vsync\n");
                }
                blitcount++;
                if (!(blitcount&7) || limitspeed)
                {
                        oldflash=readflash[0]|readflash[1]|readflash[2]|readflash[3];
//
                        if ((noborders || hires) && !fullscreen)
                        {
//                                xxl = vidc.x_min;
//                                xxh = vidc.x_max;
//                                rpclog("((noborders || hires) && !fullscreen)\n");
//                                rpclog("XXH %i XXL %i YL %i YH %i   %i,%i  %i %i %i %i  %i,%i\n",xxh,xxl,yl,yh,xxl-xxh,yh-yl,oldxxh,oldxxl,oldyl,oldyh,oldxxl-oldxxh,oldyh-oldyl);
                                if ((vidc.x_max - vidc.x_min) != (oldxxl-oldxxh) || (yh-yl)!=(oldyh-oldyl))
                                {
//                                        rpclog("Changing size!\n");
/*                                        if (vidc.scanrate || !dblscan)
                                           updatewindowsize(xxl-xxh,yh-yl);
                                        else
                                           updatewindowsize(xxl-xxh,((yh-yl)<<1)-2);*/
                                        if (vidc.scanrate || !dblscan)
                                                updatewindowsize(vidc.x_max - vidc.x_min, yh-yl);
                                        else
                                                updatewindowsize(vidc.x_max - vidc.x_min, ((yh-yl)<<1)-2);
                                }
                                oldxxl = vidc.x_min;
                                oldxxh = vidc.x_max;
                                oldyl=yl;
                                oldyh=yh;
//                                blit(buffer, vbuf, xxh, yl, xxh, yl, xxl - xxh, yh - yl);
                                blit(buffer, vbuf, vidc.x_min, yl, 0, yl, vidc.x_max - vidc.x_min, yh - yl);
                                if (vidc.scanrate || !dblscan)
                                {
//                                        blit(vbuf,bout,xxh,yl,0,0,xxl-xxh,yh-yl);
                                        blit(vbuf,bout,0,yl,0,0,vidc.x_max - vidc.x_min,yh-yl);
                                }
                                else
                                {
//                                        xxl+=32;
//                                        yh++;
                                        if (((yl<<1)-24)<0)
                                           yl=12;
                                        if ((xxh-60)<0)
                                           xxh=60;
                                        if ((xxl-60)>=672)
                                           xxl=672+60;
                                        if (((yh<<1)-24)>=544)
                                           yh=272+12;
//                                        stretch_blit(vbuf,bout,xxh,yl,xxl-xxh,(yh-yl), 0,0,xxl-xxh,(yh-yl)<<1);
                                        stretch_blit(vbuf,bout, 0,yl, vidc.x_max - vidc.x_min,(yh-yl), 0,0, vidc.x_max - vidc.x_min,(yh-yl)<<1);
                                }
                                xxl=xxh=yl=yh=-1;
                        }
                        else if (fullborders|fullscreen)
                        {
				int blit_x_offset;
				
				if (vidcr[0x38] & 0x2)
					blit_x_offset = vidc.htot + vidc.sync - 400;
				else
					blit_x_offset = vidc.htot*2 + vidc.sync*2 - 400;
				
                                oldyl=yl;
                                oldyh=yh;
                                if (redrawall || 1)
                                {
                                        if (vidc.scanrate || !dblscan)
                                        {
                                                blit(buffer, vbuf, vidc.x_mid-400, 0, 0, 0, 800, 600);
                                                blit(vbuf, bout, 0, 0, 0, 0, 800, 600);
                                        }
                                        else
                                        {
                                                blit(buffer, vbuf, vidc.x_mid-400, 0, 0, 0, 800, 300);
                                                stretch_blit(vbuf, bout, 0, 0, 800, 300, 0, 0, 800, 600);
                                        }

                                        if (redrawall)
                                                redrawall--;
                                }
                                else
                                {
                                        xxl += 32;
                                        blit(buffer, vbuf, xxh, yl, xxh, yl, xxl-xxh, yh-yl);
                                        if (vidc.scanrate || !dblscan)
                                           blit(vbuf,bout,xxh,yl,xxh,yl,xxl-xxh,yh-yl);
                                        else
                                        {
                                                xxl+=32;
                                                yh++;
                                                if ((yl<<1)<0)
                                                   yl=0;
                                                if (xxh<0)
                                                   xxh=0;
                                                if (xxl>=800)
                                                   xxl=800;
                                                if ((yh<<1)>=600)
                                                   yh=300;

                                                stretch_blit(vbuf,bout,xxh,yl,xxl-xxh,(yh-yl), xxh,(yl<<1),xxl-xxh,(yh-yl)<<1);
                                        }
                                }
                                xxh=xxl=yh=yl=-1;
                        }
                        else
                        {
                                oldyl=yl;
                                oldyh=yh;
                                if (redrawall)
                                {
                                        xxh=60;
                                        yl=24;
                                        xxl=732;
                                        yh=568;
                                        if (vidc.scanrate)// || !dblscan)
                                        {
//                                                yl-=16;
//                                                yh-=16;
                                        }
                                        else if (dblscan)
                                        {
                                                yl=12;
                                                yh=284;
                                        }
//                                        rpclog("REDRAW ALL %i,%i %i,%i %i,%i\n",xxh,yl,xxl,yh,xxl-xxh,yh-yl);
                                }
                                if (vidc.scanrate || !dblscan)
                                {
//                                        rpclog("Blit from %i,%i to %i,%i\n",xxh-60,yl-24,xxl-xxh,yh-yl);
                                        blit(buffer, vbuf, xxh, yl, xxh, yl, xxl-xxh, yh-yl);
                                        blit(vbuf,bout,xxh,yl,xxh-60,yl-24,xxl-xxh,yh-yl);
                                }
                                else
                                {
                                        xxl+=32;
                                        yh++;
                                        if (((yl<<1)-24)<0)
                                           yl=12;
                                        if ((xxh-60)<0)
                                           xxh=60;
                                        if ((xxl-60)>=672)
                                           xxl=672+60;
                                        if (((yh<<1)-24)>=544)
                                           yh=272+12;
//                                        rpclog("Blit %i,%i (%i,%i) to %i,%i (%i,%i)\n",xxh,yl,xxl-xxh,yh-yl,xxh-60,(yl<<1)-24,xxl-xxh,((yh-yl)<<1)-1);
                                        blit(buffer, vbuf, xxh, yl, xxh, yl, xxl-xxh, yh-yl);
                                        stretch_blit(vbuf,bout,xxh,yl,xxl-xxh,(yh-yl), xxh-60,(yl<<1)-24,xxl-xxh,(yh-yl)<<1);
                                }
                                if (redrawall) redrawall--;
                                xxh=xxl=yh=yl=-1;
                        }
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
                        if (fullborders || fullscreen)
                        {
                                startb=0;
                                endb=799;
                        }
                        else
                        {
                                startb=60;
                                endb=731;
                        }
                        rpclog("Blit\n");
                }
                vidc.line=0;
//                rpclog("%i fetches\n", vidc_fetches);
                vidc_fetches = 0;
                if (redrawall)
                {
                        xx=(!vidc.scanrate && !dblscan)?2:1;
                        for (x=0;x<oldyl;x+=xx)
                        {
                                bp = buffer->line[x];
                                archline(bp,0,x,799,vidc.pal[0x10]);
                        }
                        if (oldyh>0)
                        {
                                for (x=oldyh;x<600;x+=xx)
                                {
                                        bp = buffer->line[x];
                                        if (bp) archline(bp,0,x,799,vidc.pal[0x10]);
                                }
                        }
                }
                vidc_framecount++;

                vidc.x_min = 9999;
                vidc.x_max = 0;
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
