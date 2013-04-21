/*Arculator 0.8 by Tom Walker
  VIDC10 emulation*/
#include <stdio.h>
#include <allegro.h>
#include <winalleg.h>
#include "arc.h"

int rescanrate;
int framecycs=0;
/*b - memory buffer
  b2 - DirectX buffer (used for hardware blitting)*/
BITMAP /**b,*/*b2;
int depth32;
int framecount=0;
int cyclesperline=0;
int redrawall;
int flyback,flybacklines=0;
int deskdepth;
int colourconv;
int videodma=0;
int palchange;
unsigned long vidlookup[256];   /*Lookup table for 4bpp modes*/

float sampdiff;
unsigned long sdif;

struct
{
        unsigned long vtot,htot,vsync;
        int line;
        int displayon,borderon;
        unsigned long addr,caddr;
        int cycs;
        int vbstart,vbend;
        int vdstart,vdend;
        int hbstart,hbend;
        int hdstart,hdend;
        int hdstart2,hdend2;
        int sync,inter;
        /*Palette lookups - pal8 for 8bpp modes, pal for all others*/
        unsigned long pal[32],pal8[256];
        int cx,cys,cye,cxh;
        int blanking;
        int scanrate;
} vidc;

int getline()
{
//        if (vidc.scanrate) return vidc.line>>1;
        return vidc.line;
}
unsigned long monolook[16][4];
unsigned long hirescurcol[4]={0,0,0,0xFFFFFF};

void redolookup()
{
        int c;
        if (hires)
        {
                for (c=0;c<16;c++)
                {
                        if (depth32)
                        {
                                monolook[c][0]=(vidcr[c]&1)?0xFFFFFF:0x000000;
                                monolook[c][1]=(vidcr[c]&2)?0xFFFFFF:0x000000;
                                monolook[c][2]=(vidcr[c]&4)?0xFFFFFF:0x000000;
                                monolook[c][3]=(vidcr[c]&8)?0xFFFFFF:0x000000;
                        }
                        else
                        {
                                monolook[c][0]=(vidcr[c]&1)?0xFFFF:0x0000;
                                monolook[c][0]|=(vidcr[c]&2)?0xFFFF0000:0x00000000;
                                monolook[c][1]=(vidcr[c]&4)?0xFFFF:0x0000;
                                monolook[c][1]|=(vidcr[c]&8)?0xFFFF0000:0x00000000;
                        }
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

int redrawpalette=0;
void writevidc(unsigned long v)
{
//        char s[80];
        RGB r;
        int c,d,oldscanrate=vidc.scanrate,oldvtot=vidc.vtot;
//        rpclog("Write VIDC %08X %07X\n",v,PC);
        if ((v>>26)==0x38 && v!=vidcr[0x38]) redrawall=(fullscreen)?4:2;
        if (((v>>24)&~0x1F)==0x60)
        {
                stereoimages[((v>>26)-1)&7]=v&7;
//                rpclog("Stereo image write %08X %i %i\n",v,((v>>26)-1)&7,v&7);
        }
        if (((v>>26)<0x14) && (v!=vidcr[v>>26] || redrawpalette))
        {
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
                vidc.htot=(v&0xFFFFFF)>>14;
                cyclesperline=0;
        }
        if ((v>>24)==0xA0)
        {
                vidc.vtot=((v&0xFFFFFF)>>14)+1;
                if (vidc.vtot>400)
                   vidc.scanrate=1;
                else
                   vidc.scanrate=0;
                if ((vidc.scanrate!=oldscanrate) || (vidc.vtot!=oldvtot))
                {
                        clear(screen);
                        clear(b2);
//                        clear(b);
                        redrawall=(fullscreen)?4:2;
                        rescanrate=1;
                }
        }
        if ((v>>24)==0x84) vidc.sync=(v&0xFFFFFF);
        if ((v>>24)==0x88) vidc.hbstart=(((v&0xFFFFFF)>>14)<<1)+1;
        if ((v>>24)==0x8C)
        {
                vidc.hdstart2=((v&0xFFFFFF)>>14);
                recalcse();
        }
        if ((v>>24)==0x90)
        {
                vidc.hdend2=((v&0xFFFFFF)>>14);
                recalcse();
        }
        if ((v>>24)==0x94) vidc.hbend=(((v&0xFFFFFF)>>14)<<1)+1;
        if ((v>>24)==0x98) { vidc.cx=((v&0xFFE000)>>13)+6; vidc.cxh=((v&0xFFF800)>>11)+24; }
        if ((v>>24)==0x9C) vidc.inter=(v&0xFFFFFF);
        if ((v>>24)==0xA4) vidc.vsync=((v&0xFFFFFF)>>14)+1;
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
                soundhz=250000/((v&0xFF)+2);
                soundper=((v&0xFF)+2);//8000000/soundhz;
                sampdiff=200.0f/(float)soundper;
                sdif=(int)((float)sampdiff*4096.0f);
//                rpclog("Sound frequency write %08X period %i\n",v,soundper);
        }
        if ((v>>24)==0xE0)
        {
                recalcse();
                cyclesperline=0;
        }
//        printf("VIDC write %08X\n",v);
}

void clearbitmap()
{
        redrawall=(fullscreen)?4:2;
//        clear(b);
        clear(b2);
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
        if (depth==16 || depth==15)
        {
                set_color_depth(16);
                if (set_gfx_mode(GFX_AUTODETECT_WINDOWED,1152,864,0,0))
                {
                        set_color_depth(15);
                        depth=15;
                        set_gfx_mode(GFX_AUTODETECT_WINDOWED,1152,864,0,0);
                }
        }
        else if (depth==32)
        {
                set_color_depth(32);
                set_gfx_mode(GFX_AUTODETECT_WINDOWED,1152,864,0,0);
        }
        b2=create_system_bitmap(1536,1536);
        depth32=(depth==32)?16:0;
//        if (depth!=15) set_color_depth(16);
//        else           set_color_depth(15);
//        set_color_depth(8);
//        b=create_bitmap(1024,1024);
        vidc.line=0;
//        clear(b);
        clear(b2);
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

int oldxxh,oldxxl,oldyh,oldyl;
void setredrawall()
{
        oldxxh=oldxxl=oldyh=oldyl=-1;
                redrawall=(fullscreen)?4:2;
//        clear(b);
        clear(b2);
}

void closevideo()
{
        destroy_bitmap(b2);
//        destroy_bitmap(b);
        allegro_exit();
}

BITMAP *vbufs[3];
int drawbuf=0;
void reinitvideo()
{
//        int c;
        destroy_bitmap(b2);
//        destroy_bitmap(b);
        if (fullscreen)
        {
                set_color_depth(16);
                if (hires)
                {
                        if (set_gfx_mode(GFX_AUTODETECT_FULLSCREEN,1152,896,0,0))
                        {
                                set_color_depth(15);
                                if (set_gfx_mode(GFX_AUTODETECT_FULLSCREEN,1152,896,0,0))
                                {
                                        if (set_gfx_mode(GFX_AUTODETECT_FULLSCREEN,1280,960,0,0))
                                        {
                                                set_color_depth(16);
                                                if (set_gfx_mode(GFX_AUTODETECT_FULLSCREEN,1280,960,0,0))
                                                {
                                                        if (set_gfx_mode(GFX_AUTODETECT_FULLSCREEN,1280,1024,0,0))
                                                        {
                                                                set_color_depth(15);
                                                                set_gfx_mode(GFX_AUTODETECT_FULLSCREEN,1280,1024,0,0);
                                                        }
                                                }
                                        }
                                }
                        }
                }
                else
                {
                        if (set_gfx_mode(GFX_AUTODETECT_FULLSCREEN,800,600,0,0))
                        {
                                set_color_depth(15);
                                set_gfx_mode(GFX_AUTODETECT_FULLSCREEN,800,600,0,0);
                        }
                }
                depth32=0;
                drawbuf=1;
        }
        else
        {
                if (deskdepth==16 || deskdepth==15)
                {
                        set_color_depth(16);
                        if (set_gfx_mode(GFX_AUTODETECT_WINDOWED,1152,896,0,0))
                        {
                                set_color_depth(15);
                                set_gfx_mode(GFX_AUTODETECT_WINDOWED,1152,896,0,0);
                        }
                }
                else if (deskdepth==32)
                {
                        set_color_depth(32);
                        set_gfx_mode(GFX_AUTODETECT_WINDOWED,1152,896,0,0);
                }
                depth32=(deskdepth==32)?16:0;
        }
//        b=create_bitmap(1024,1024);
        b2=create_system_bitmap(1152,1536);
        setredrawall();
        redopalette();
//        clear(b2);
//        clear(b);
}

//#if 0
int lastblack=0;

void drawvid()
{
}

FILE *vlog;
int getvidcline()
{
        return vidc.line;
}
int getvidcwidth()
{
        return vidc.htot;
}

int yh,yl,xxh,xxl;
int oldyl,oldyh;

int oldflash;

int startb,endb;

void archline(unsigned char *bp, int x1, int y, int x2, unsigned long col)
{
        int x;
        if (depth32)
        {
                for (x=x1;x<=x2;x++)
                    ((unsigned long *)bp)[x]=col;
        }
        else
        {
                for (x=x1;x<=x2;x++)
                    ((unsigned short *)bp)[x]=col;
        }
}

void pollline()
{
        int c;
        int mode;
//        int col=0;
        int x,xx,xl;//,y,c;
        unsigned long temp;
        unsigned long *p;
        unsigned char *bp;
//        char s[256];
        int l=(vidc.line-16);
        int xoffset,xoffset2;
        BITMAP *bout=screen;
        if (!vidc.scanrate && !dblscan) l<<=1;
        if (vidc.scanrate) l+=32;
        if (vidc.scanrate && vidc.vtot>600) l-=40;
        if (palchange)
        {
                redolookup();
                palchange=0;
        }
        flybacklines--;
                if (vidc.line==vidc.vdstart)
                {
//                        rpclog("VIDC addr %08X\n",vinit);
                        vidc.addr=vinit;
                        vidc.caddr=cinit;
                }
        if (vidc.line==vidc.vbstart) { vidc.borderon=0; flyback=0; /*rpclog("border off %i %i\n",vidc.line,l);*/ }
        if (vidc.line==vidc.vdstart) { vidc.displayon=1; flyback=0; if (yl==-1) yl=l; /*rpclog("Display on %i %i\n",vidc.line,l);*/ }
        if (vidc.line==vidc.vdend)
        {
                vidc.displayon=0;
                ioc.irqa|=8;
                updateirqs();
                flyback=0x80;
                flybacklines=vidc.vsync;
                if (yh==-1) yh=l;
//                rpclog("Normal vsync\n");
        }
        if (vidc.line==vidc.vbend) { vidc.borderon=1; if (yh==-1) yh=l; /*rpclog("Border on %i %i\n",vidc.line,l);*/ }
        vidc.line++;
        videodma=vidc.addr;
        mode=(vidcr[0x38]&0xF);
        if (hires) mode=2;
//#if 0
        if (l>=0 && vidc.line<=((hires)?1023:((vidc.scanrate)?800:316)) && l<1536)
        {
                x=vidc.hbstart;
                if (vidc.hdstart>x) x=vidc.hdstart;
                xx=vidc.hbend;
                if (vidc.hdend<xx) xx=vidc.hdend;
                xoffset=xx-x;
                if (!(vidcr[0x38]&2))
                {
//                        rpclog("Border - %i %i  %i %i   %i  ",vidc.hbstart,vidc.hbend,x,xx,xoffset);
                        xoffset=200-(xoffset>>1);
//                        rpclog("%i  ",xoffset);
                        if (vidc.hdstart<vidc.hbstart) xoffset2=xoffset+(vidc.hdstart-vidc.hbstart);
                        else                           xoffset2=xoffset;
//                        rpclog("%i  ",xoffset2);
                        xoffset<<=1;
                        xoffset2<<=1;
//                        rpclog("%i %i\n",xoffset,xoffset2);
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
                        bp=(unsigned char *)bmp_write_line(b2,l);
/*                        if (!bp)
                        {
                                error("Cannot get line!\n");
                                exit(-1);
                        }*/
                        switch (mode|depth32)
                        {
                                case 2: /*Mode 0*/
                                case 3: /*Mode 25*/
                                xl=xxl=(vidc.hdend-vidc.hdstart)+xoffset2;
                                x=xxh=xoffset2;
                                p=(unsigned long *)(bp+(x<<1));
                                if (hires)
                                {
                                        p=(unsigned long *)(bp+(x>>1));
                                        x<<=2;
                                        xl<<=2;
                                        xxl<<=2;
                                        xxh<<=2;
                                }
                                for (;x<xl;x+=32)
                                {
                                        temp=ram[vidc.addr++];
                                        if (x<800 || hires)
                                        {
                                                if (hires)
                                                {
                                                        for (xx=0;xx<16;xx+=2)
                                                        {
                                                                p[xx]=monolook[temp&15][0];
                                                                p[xx+1]=monolook[temp&15][1];
                                                                temp>>=4;
                                                        }
                                                }
                                                else
                                                {
                                                        for (xx=0;xx<16;xx++)
                                                            p[xx]=vidlookup[(temp>>(xx<<1))&3];
                                                }
                                                p+=16;
                                        }
                                        if (vidc.addr==vend+4) vidc.addr=vstart;
                                }
                                break;
                                case 4: /*Mode 1*/
                                case 5:
                                xl=xxl=((vidc.hdend-vidc.hdstart)<<1)+xoffset2;
                                x=xxh=xoffset2;
                                for (;x<xl;x+=32)
                                {
                                        temp=ram[vidc.addr++];
                                        if (x<800)
                                        {
                                                for (xx=0;xx<32;xx+=2)
                                                    ((unsigned short *)bp)[x+xx]=((unsigned short *)bp)[x+xx+1]=vidc.pal[(temp>>xx)&3];
                                        }
                                        if (vidc.addr==vend+4) vidc.addr=vstart;
                                }
                                break;
                                case 6: /*Mode 8*/
                                case 7: /*Mode 26*/
                                xl=xxl=(vidc.hdend-vidc.hdstart)+xoffset2;
                                x=xxh=xoffset2;
                                p=(unsigned long *)(bp+(x<<1));
                                for (;x<xl;x+=16)
                                {
                                        temp=ram[vidc.addr++];
                                        if (x<800)
                                        {
                                                for (xx=0;xx<8;xx++)
                                                    p[xx]=vidlookup[(temp>>(xx<<2))&0xF];
                                                p+=8;
                                        }
                                        if (vidc.addr==vend+4) vidc.addr=vstart;
                                }
                                break;
                                case 8: /*Mode 9*/
                                case 9:
                                xl=xxl=((vidc.hdend-vidc.hdstart)<<1)+xoffset2;
//                                rpclog("%i %i  %i  %i\n",vidc.hdstart,vidc.hdend,vidc.hdend-vidc.hdstart,xoffset2);
                                x=xxh=xoffset2;
                                p=(unsigned long *)(bp+(x<<1));
                                xl-=x;
                                xl>>=1;
                                xx=x;
                                for (x=0;x<xl;x+=8)
                                {
                                        temp=ram[vidc.addr++];
                                        if (x<(800-xx))
                                        {
                                                for (c=0;c<8;c++)
                                                    p[c]=vidlookup[(temp>>(c<<2))&0xF];
                                                p+=8;
                                        }
                                        if (vidc.addr==vend+4) vidc.addr=vstart;
                                }
                                xl=((vidc.hdend-vidc.hdstart)<<1)+xoffset2;
//                                p2[0]=0xFFFFFFFF;
//                                p--;
//                                p[0]=0xFFFFFFFF;
                                break;
                                case 10: /*Mode 12*/
                                case 11: /*Mode 27*/
                                xl=xxl=(vidc.hdend-vidc.hdstart)+xoffset2;
                                x=xxh=xoffset2;
                                p=(unsigned long *)(bp+(x<<1));
                                xl-=x;
                                xl>>=1;
                                xx=x;
                                for (x=0;x<xl;x+=4)
                                {
                                        temp=ram[vidc.addr++];
                                        if (x<(800-xx))
                                        {
                                                for (c=0;c<4;c++)
                                                    p[c]=vidlookup[(temp>>(c<<3))&0xFF];
                                                p+=4;
                                        }
                                        if (vidc.addr==vend+4) vidc.addr=vstart;
                                }
                                xl=(vidc.hdend-vidc.hdstart)+xoffset2;
                                break;
                                case 12: /*Mode 13*/
                                case 13:
                                xl=xxl=((vidc.hdend-vidc.hdstart)<<1)+xoffset2;
                                x=xxh=xoffset2;
                                for (;x<xl;x+=8)
                                {
                                        temp=ram[vidc.addr++];
                                        if (x<800)
                                        {
                                                ((unsigned short *)bp)[x]=((unsigned short *)bp)[x+1]=vidc.pal8[temp&0xFF];
                                                ((unsigned short *)bp)[x+2]=((unsigned short *)bp)[x+3]=vidc.pal8[(temp>>8)&0xFF];
                                                ((unsigned short *)bp)[x+4]=((unsigned short *)bp)[x+5]=vidc.pal8[(temp>>16)&0xFF];
                                                ((unsigned short *)bp)[x+6]=((unsigned short *)bp)[x+7]=vidc.pal8[(temp>>24)&0xFF];
                                        }
                                        if (vidc.addr==vend+4) vidc.addr=vstart;
                                }
                                break;
                                case 14: /*Mode 15*/
                                case 15: /*Mode 28*/
                                xl=xxl=(vidc.hdend-vidc.hdstart)+xoffset2;
                                x=xxh=xoffset2;
                                for (;x<xl;x+=4)
                                {
                                        temp=ram[vidc.addr++];
                                        if (x<800)
                                        {
                                                ((unsigned short *)bp)[x]=vidc.pal8[temp&0xFF];
                                                ((unsigned short *)bp)[x+1]=vidc.pal8[(temp>>8)&0xFF];
                                                ((unsigned short *)bp)[x+2]=vidc.pal8[(temp>>16)&0xFF];
                                                ((unsigned short *)bp)[x+3]=vidc.pal8[(temp>>24)&0xFF];
                                        }
                                        if (vidc.addr==vend+4) vidc.addr=vstart;
                                }
                                break;

                                case 16+2: /*Mode 0*/
                                case 16+3: /*Mode 25*/
                                xl=xxl=(vidc.hdend-vidc.hdstart)+xoffset2;
                                x=xxh=xoffset2;
                                p=(unsigned long *)(bp+(x<<2));
                                if (hires)
                                {
                                        p=(unsigned long *)(bp);
/*                                        x<<=2;
                                        xl<<=2;*/
//                                        rpclog("xxl %i xxh %i\n",xxl,xxh);
                                        xxl<<=2;
                                        xxh<<=2;
                                }
                                for (;x<xl;x+=8)
                                {
                                        temp=ram[vidc.addr++];
                                        if (x<800)
                                        {
                                                if (hires)
                                                {
                                                        for (xx=0;xx<32;xx+=4)
                                                        {
                                                                p[xx]=monolook[temp&0xF][0];
                                                                p[xx+1]=monolook[temp&0xF][1];
                                                                p[xx+2]=monolook[temp&0xF][2];
                                                                p[xx+3]=monolook[temp&0xF][3];
                                                                temp>>=4;
                                                        }
//                                                            p[xx]=((temp>>xx)&1)?0x000000:0xFFFFFF;
                                                }
                                                else
                                                {
                                                        for (xx=0;xx<32;xx++)
                                                            p[xx]=vidc.pal[(temp>>xx)&1];
                                                }
                                                p+=32;
                                        }
                                        if (vidc.addr==vend+4) vidc.addr=vstart;
                                }
                                break;
                                case 16+4: /*Mode 1*/
                                case 16+5:
                                xl=xxl=((vidc.hdend-vidc.hdstart)<<1)+xoffset2;
                                x=xxh=xoffset2;
                                for (;x<xl;x+=32)
                                {
                                        temp=ram[vidc.addr++];
                                        if (x<800)
                                        {
                                                for (xx=0;xx<32;xx+=2)
                                                    ((unsigned long *)bp)[x+xx]=((unsigned long *)bp)[x+xx+1]=vidc.pal[(temp>>xx)&3];
                                        }
                                        if (vidc.addr==vend+4) vidc.addr=vstart;
                                }
                                break;
                                case 16+6: /*Mode 8*/
                                case 16+7: /*Mode 26*/
                                xl=xxl=(vidc.hdend-vidc.hdstart)+xoffset2;
                                x=xxh=xoffset2;
                                p=(unsigned long *)(bp+(x<<2));
                                for (;x<xl;x+=16)
                                {
                                        temp=ram[vidc.addr++];
                                        if (x<800)
                                        {
                                                for (xx=0;xx<16;xx++)
                                                    p[xx]=vidc.pal[temp>>(xx<<1)&3];
                                                p+=16;
                                        }
                                        if (vidc.addr==vend+4) vidc.addr=vstart;
                                }
                                break;
                                case 16+8: /*Mode 9*/
                                case 16+9:
                                xl=xxl=((vidc.hdend-vidc.hdstart)<<1)+xoffset2;
                                x=xxh=xoffset2;
                                p=(unsigned long *)(bp+(x<<2));
                                xl-=x;
//                                xl>>=1;
                                xx=x;
                                for (x=0;x<xl;x+=16)
                                {
                                        temp=ram[vidc.addr++];
                                        if (x<(800-xx))
                                        {
                                                for (c=0;c<16;c+=2)
                                                    p[c]=p[c+1]=vidc.pal[(temp>>(c<<1))&0xF];
                                                p+=16;
                                        }
                                        if (vidc.addr==vend+4) vidc.addr=vstart;
                                }
                                xl=((vidc.hdend-vidc.hdstart)<<1)+xoffset2;
                                break;
                                case 16+10: /*Mode 12*/
                                case 16+11: /*Mode 27*/
                                xl=xxl=(vidc.hdend-vidc.hdstart)+xoffset2;
                                x=xxh=xoffset2;
                                p=(unsigned long *)(bp+(x<<2));
                                xl-=x;
//                                xl>>=1;
                                xx=x;
                                for (x=0;x<xl;x+=8)
                                {
                                        temp=ram[vidc.addr++];
                                        if (x<(800-xx))
                                        {
                                                for (c=0;c<8;c++)
                                                    p[c]=vidc.pal[(temp>>(c<<2))&0xF];
                                                p+=8;
                                        }
                                        if (vidc.addr==vend+4) vidc.addr=vstart;
                                }
                                xl=(vidc.hdend-vidc.hdstart)+xoffset2;
                                break;
                                case 16+12: /*Mode 13*/
                                case 16+13:
                                xl=xxl=((vidc.hdend-vidc.hdstart)<<1)+xoffset2;
                                x=xxh=xoffset2;
                                for (;x<xl;x+=8)
                                {
                                        temp=ram[vidc.addr++];
                                        if (x<800)
                                        {
                                                ((unsigned long *)bp)[x]=((unsigned long *)bp)[x+1]=vidc.pal8[temp&0xFF];
                                                ((unsigned long *)bp)[x+2]=((unsigned long *)bp)[x+3]=vidc.pal8[(temp>>8)&0xFF];
                                                ((unsigned long *)bp)[x+4]=((unsigned long *)bp)[x+5]=vidc.pal8[(temp>>16)&0xFF];
                                                ((unsigned long *)bp)[x+6]=((unsigned long *)bp)[x+7]=vidc.pal8[(temp>>24)&0xFF];
                                        }
                                        if (vidc.addr==vend+4) vidc.addr=vstart;
                                }
                                break;
                                case 16+14: /*Mode 15*/
                                case 16+15: /*Mode 28*/
                                xl=xxl=(vidc.hdend-vidc.hdstart)+xoffset2;
                                x=xxh=xoffset2;
                                for (;x<xl;x+=4)
                                {
                                        temp=ram[vidc.addr++];
                                        if (x<800)
                                        {
                                                ((unsigned long *)bp)[x]=vidc.pal8[temp&0xFF];
                                                ((unsigned long *)bp)[x+1]=vidc.pal8[(temp>>8)&0xFF];
                                                ((unsigned long *)bp)[x+2]=vidc.pal8[(temp>>16)&0xFF];
                                                ((unsigned long *)bp)[x+3]=vidc.pal8[(temp>>24)&0xFF];
                                        }
                                        if (vidc.addr==vend+4) vidc.addr=vstart;
                                }
                                break;

                                default:
                                textprintf(b2,font,0,0,0x7FFF,"%i",(int)(vidcr[0x38]&0xF));
                                xl=0;
                        }
//                        #if 0
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
                                   archline(bp,startb,l,xoffset2-1,vidc.pal[0x10]);
                                else
                                {
                                        if (noborders && !fullscreen) xxh=xoffset-1;
                                        archline(bp,startb,l,xoffset-1,vidc.pal[0x10]);
                                }
                                if ((xoffset+((vidc.hbend-vidc.hbstart)<<1))<xl)
                                {
                                        if (noborders && !fullscreen) xxl=xoffset+((vidc.hbend-vidc.hbstart)<<1);
                                        archline(bp,xoffset+((vidc.hbend-vidc.hbstart)<<1),l,endb,vidc.pal[0x10]);
                                }
                                else
                                   archline(bp,xl,l,endb,vidc.pal[0x10]);
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
                                   archline(bp,startb,l,xoffset2-1,vidc.pal[0x10]);
                                else
                                {
                                        if (noborders && !fullscreen) xxh=xoffset-1;
                                        archline(bp,startb,l,xoffset-1,vidc.pal[0x10]);
                                }
                                if (vidc.hbend<xl)
                                {
                                        if (noborders && !fullscreen) xxl=xoffset+(vidc.hbend-vidc.hbstart);
                                        archline(bp,xoffset+(vidc.hbend-vidc.hbstart),l,endb,vidc.pal[0x10]);
                                }
                                else
                                   archline(bp,xl,l,endb,vidc.pal[0x10]);
                                break;
                        }
                        if (((vidc.cys>>14)+2)<=vidc.line && ((vidc.cye>>14)+2)>vidc.line)
                        {
                                if (hires)
                                {
                                        x=(vidc.cxh-(vidc.hdstart<<2))-30;
                                        if (depth32)
                                        {
                                                temp=ram[vidc.caddr++];
                                                for (xx=0;xx<32;xx+=2)
                                                {
                                                        if (temp&3) ((unsigned long *)bp)[x+xx+1]=((unsigned long *)bp)[x+xx]=hirescurcol[temp&3];
                                                        temp>>=2;
                                                }
                                                temp=ram[vidc.caddr++];
                                                for (xx=32;xx<64;xx+=2)
                                                {
                                                        if (temp&3) ((unsigned long *)bp)[x+xx+1]=((unsigned long *)bp)[x+xx]=hirescurcol[temp&3];
                                                        temp>>=2;
                                                }
                                        }
                                        else
                                        {
                                                temp=ram[vidc.caddr++];
                                                for (xx=0;xx<32;xx+=2)
                                                {
                                                        if (temp&3) ((unsigned short *)bp)[x+xx+1]=((unsigned short *)bp)[x+xx]=hirescurcol[temp&3];
                                                        temp>>=2;
                                                }
                                                temp=ram[vidc.caddr++];
                                                for (xx=32;xx<64;xx+=2)
                                                {
                                                        if (temp&3) ((unsigned short *)bp)[x+xx+1]=((unsigned short *)bp)[x+xx]=hirescurcol[temp&3];
                                                        temp>>=2;
                                                }
                                        }
                                }
                                else switch ((vidcr[0x38]&0xF)|depth32)
                                {
                                        case 0: /*Mode 4*/
                                        case 1:
                                        case 4: /*Mode 1*/
                                        case 5:
                                        case 8: /*Mode 9*/
                                        case 9:
                                        case 12: /*Mode 13*/
                                        case 13:
                                        x=((vidc.cx-vidc.hdstart)<<1)+xoffset2;
                                        if (x>800) break;
                                        temp=ram[vidc.caddr++];
                                        for (xx=0;xx<32;xx+=2)
                                        {
                                                if (temp&3) ((unsigned short *)bp)[x+xx]=((unsigned short *)bp)[x+xx+1]=vidc.pal[(temp&3)|0x10];
                                                temp>>=2;
                                        }
                                        temp=ram[vidc.caddr++];
                                        for (xx=32;xx<64;xx+=2)
                                        {
                                                if (temp&3) ((unsigned short *)bp)[x+xx]=((unsigned short *)bp)[x+xx+1]=vidc.pal[(temp&3)|0x10];
                                                temp>>=2;
                                        }
                                        break;
                                        case 16+0: /*Mode 4*/
                                        case 16+1:
                                        case 16+4: /*Mode 1*/
                                        case 16+5:
                                        case 16+8: /*Mode 9*/
                                        case 16+9:
                                        case 16+12: /*Mode 13*/
                                        case 16+13:
                                        x=((vidc.cx-vidc.hdstart)<<1)+xoffset2;
                                        if (x>800) break;
                                        temp=ram[vidc.caddr++];
                                        for (xx=0;xx<32;xx+=2)
                                        {
                                                if (temp&3) ((unsigned long *)bp)[x+xx]=((unsigned long *)bp)[x+xx+1]=vidc.pal[(temp&3)|0x10];
                                                temp>>=2;
                                        }
                                        temp=ram[vidc.caddr++];
                                        for (xx=32;xx<64;xx+=2)
                                        {
                                                if (temp&3) ((unsigned long *)bp)[x+xx]=((unsigned long *)bp)[x+xx+1]=vidc.pal[(temp&3)|0x10];
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
                                        x=(vidc.cx-vidc.hdstart)+xoffset2;
                                        if (x>800) break;
                                        temp=ram[vidc.caddr++];
                                        for (xx=0;xx<16;xx++)
                                        {
                                                if (temp&3) ((unsigned short *)bp)[x+xx]=vidc.pal[(temp&3)|0x10];
                                                temp>>=2;
                                        }
                                        temp=ram[vidc.caddr++];
                                        for (xx=16;xx<32;xx++)
                                        {
                                                if (temp&3) ((unsigned short *)bp)[x+xx]=vidc.pal[(temp&3)|0x10];
                                                temp>>=2;
                                        }
                                        break;
                                        case 16+2:  /*Mode 0*/
                                        case 16+3:  /*Mode 25*/
                                        case 16+6:  /*Mode 8*/
                                        case 16+7:  /*Mode 26*/
                                        case 16+10: /*Mode 12*/
                                        case 16+11: /*Mode 27*/
                                        case 16+14: /*Mode 15*/
                                        case 16+15: /*Mode 28*/
                                        x=(vidc.cx-vidc.hdstart)+xoffset2;
//                                        rpclog("CX %i HDSTART %i\n",vidc.cx,vidc.hdstart);
                                        if (x>800) break;
                                                temp=ram[vidc.caddr++];
                                                for (xx=0;xx<16;xx++)
                                                {
                                                        if (temp&3) ((unsigned long *)bp)[x+xx]=vidc.pal[(temp&3)|0x10];
                                                        temp>>=2;
                                                }
                                                temp=ram[vidc.caddr++];
                                                for (xx=16;xx<32;xx++)
                                                {
                                                        if (temp&3) ((unsigned long *)bp)[x+xx]=vidc.pal[(temp&3)|0x10];
                                                        temp>>=2;
                                                }
                                        break;
                                }
                        }
//                        #endif
                }
                if (vidc.blanking && redrawall && !((noborders || hires) && !fullscreen))
                {
                        if (!vidc.displayon)
                           bp=(unsigned char *)bmp_write_line(b2,l);
                        archline(bp,startb,l,799,0);
//                        if (dblscan && !vidc.scanrate)
//                           archline(bp,startb,l+1,799,0);
                }
                else if ((vidc.borderon || !vidc.displayon) && redrawall && !((noborders || hires) && !fullscreen))
                {
                        if (!vidc.displayon)
                           bp=(unsigned char *)bmp_write_line(b2,l);
                        archline(bp,startb,l,799,vidc.pal[16]);
//                        if (dblscan && !vidc.scanrate)
//                           archline(bp,startb,l+1,799,vidc.pal[16]);
                }
        }
        videodma=(vidc.addr-videodma);
        if (videodma>0)
        {
                videodma>>=2;
                videodma*=5;
                videodma<<=1;
        }
        else
           videodma=0;

        if (vidc.line>=vidc.vtot)
        {
                vidc.blanking=0;
//                rpclog("Frame over! %i %i %i %i\n",vidc.line,vidc.vtot,vidc.displayon,framecycs);
                framecycs=0;
                if (vidc.displayon)
                {
                        vidc.displayon=0;
                        ioc.irqa|=8;
                        updateirqs();
                        flyback=0x80;
                        flybacklines=vidc.vsync;
                        yh=l;
//                        rpclog("Late vsync\n");
                }
//                if (fullscreen) bout=vbufs[drawbuf];
                        framecount++;
                        if (!redrawall && oldflash^(readflash[0]|readflash[1]|readflash[2]|readflash[3]))
                        {
                                if (fullborders|fullscreen)
                                {
                                        if (!readflash[0]) rectfill(bout,780,4,796,8,vidc.pal[16]);
                                        if (!readflash[1]) rectfill(bout,760,4,776,8,vidc.pal[16]);
                                        if (!readflash[2]) rectfill(bout,740,4,756,8,vidc.pal[16]);
                                        if (!readflash[3]) rectfill(bout,720,4,736,8,vidc.pal[16]);
                                        if (!dblscan && !vidc.scanrate) hline(bout,720,5,796,0);
                                        if (!dblscan && !vidc.scanrate) hline(bout,720,7,796,0);
                                }
                                else
                                {
                                        if (!readflash[0]) rectfill(bout,652,4,668,8,vidc.pal[16]);
                                        if (!readflash[1]) rectfill(bout,632,4,648,8,vidc.pal[16]);
                                        if (!readflash[2]) rectfill(bout,612,4,628,8,vidc.pal[16]);
                                        if (!readflash[3]) rectfill(bout,592,4,608,8,vidc.pal[16]);
                                        if (!dblscan && !vidc.scanrate) hline(bout,592,5,668,0);
                                        if (!dblscan && !vidc.scanrate) hline(bout,592,7,668,0);
                                }
                        }
                        oldflash=readflash[0]|readflash[1]|readflash[2]|readflash[3];
                        if ((noborders || hires) && !fullscreen)
                        {
//                                rpclog("XXH %i XXL %i YL %i YH %i   %i,%i  %i %i %i %i  %i,%i\n",xxh,xxl,yl,yh,xxl-xxh,yh-yl,oldxxh,oldxxl,oldyl,oldyh,oldxxl-oldxxh,oldyh-oldyl);
                                if ((xxl-xxh)!=(oldxxl-oldxxh) || (yh-yl)!=(oldyh-oldyl))
                                {
//                                        rpclog("Changing size!\n");
                                        if (vidc.scanrate || !dblscan)
                                           updatewindowsize(xxl-xxh,yh-yl);
                                        else
                                           updatewindowsize(xxl-xxh,((yh-yl)<<1)-2);
                                }
                                oldxxl=xxl;
                                oldxxh=xxh;
                                oldyl=yl;
                                oldyh=yh;
                                if (vidc.scanrate || !dblscan)
                                {
                                        blit(b2,bout,xxh,yl,0,0,xxl-xxh,yh-yl);
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
                                        stretch_blit(b2,bout,xxh,yl,xxl-xxh,(yh-yl), 0,0,xxl-xxh,((yh-yl)<<1)-1);
                                }
                                xxl=xxh=yl=yh=-1;
                        }
                        else if (fullborders|fullscreen)
                        {
                                oldyl=yl;
                                oldyh=yh;
                                if (redrawall)
                                {
                                        if (vidc.scanrate || !dblscan)
                                           blit(b2,bout,0,0,0,0,800,600);
                                        else
                                           stretch_blit(b2,bout,0,0,800,300,0,0,800,599);
                                        redrawall--;
                                }
                                else
                                {
                                        if (vidc.scanrate || !dblscan)
                                           blit(b2,bout,xxh,yl,xxh,yl,xxl-xxh,yh-yl);
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
//                                                rpclog("fBlit %i,%i (%i,%i) to %i,%i (%i,%i)\n",xxh,yl,xxl-xxh,yh-yl,xxh,(yl<<1),xxl-xxh,((yh-yl)<<1)-1);
                                                stretch_blit(b2,bout,xxh,yl,xxl-xxh,(yh-yl), xxh,(yl<<1),xxl-xxh,((yh-yl)<<1)-1);
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
                                        blit(b2,bout,xxh,yl,xxh-60,yl-24,xxl-xxh,yh-yl);
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
                                        stretch_blit(b2,bout,xxh,yl,xxl-xxh,(yh-yl), xxh-60,(yl<<1)-24,xxl-xxh,((yh-yl)<<1)-1);
                                }
                                if (redrawall) redrawall--;
                                xxh=xxl=yh=yl=-1;
                        }
                        if (fullborders|fullscreen)
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
                        readflash[0]=readflash[1]=readflash[2]=readflash[3]=0;
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
                /*if (fullscreen)
                {
                        request_video_bitmap(vbufs[drawbuf]);
                        drawbuf++;
                        if (drawbuf==3) drawbuf=0;
                }*/
                vidc.line=0;
                if (redrawall)
                {
                        xx=(!vidc.scanrate && !dblscan)?2:1;
                        for (x=0;x<oldyl;x+=xx)
                        {
                                bp=bmp_write_line(b2,x);
                                archline(bp,0,x,799,vidc.pal[0x10]);
                        }
                        if (oldyh>0)
                        {
                                for (x=oldyh;x<600;x+=xx)
                                {
                                        bp=bmp_write_line(b2,x);
                                        if (bp) archline(bp,0,x,799,vidc.pal[0x10]);
                                }
                        }
                        bmp_unwrite_line(b2);
/*                        rescanrate=0;
                        clear_to_color(b2,vidc.pal[0x10]);
                        if (!vidc.scanrate && !dblscan)
                        {
                                for (x=1;x<600;x+=2)
                                {
                                        bp=bmp_write_line(b2,x);
                                        archline(bp,0,x,799,0);
                                }
                                bmp_unwrite_line(b2);
                        }*/
                }
        }
}

void redovideotiming()
{
        cyclesperline=0;
}
int vidcgetcycs()
{
        int temp;
        if (cyclesperline)
        {
                return cyclesperline-videodma;
//                if (vidc.displayon) return cyclesperline-200;
//                return cyclesperline;
        }
        temp=(vidc.htot+1)<<2;
//        rpclog("Vidcycs %i ",temp);
        switch (vidcr[0x38]&3)
        {
                case 0: temp>>=1; break;
                case 1: temp/=3; break;
                case 2: temp>>=2; break;
                case 3: temp/=6; break;
        }
//        rpclog("%i  ",temp);
//        if (vidcr[0x38]&2) temp>>=1;
        vidc.cycs=temp<<1;
        if (!speed) cyclesperline=temp<<1;
        else if (speed==1) cyclesperline=(temp*3);
        else if (speed==2) cyclesperline=(temp*6);
        else cyclesperline=(temp<<3);
//        rpclog("%i  %i  %i  %i\n",cyclesperline,vidc.vtot,speed,videodma);
        return cyclesperline;
}

int vidcgetoverflow()
{
        return videodma;
//        if (cyclesperline && vidc.displayon) return videodma;
        return 0;
}

int vidcgetendline()
{
        int temp=(vidc.hbend+1)<<1;
        if (vidcr[0x38]&2) temp>>=1;
//        vidc.cycs=temp<<1;
        if (!speed) return temp<<1;
        else if (speed==1) return temp*3;
        else if (speed==2) return temp*6;
        return temp<<3;
}
