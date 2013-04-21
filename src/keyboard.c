/*Arculator 0.8 by Tom Walker
  Keyboard/mouse emulation*/
#include <allegro.h>
#include <stdio.h>
#include "arc.h"
#include "keytable.h"

int mousecapture;
char bigs[256];
FILE *olog;

int keydat[128];

int moldx,moldy;
int ledcaps,lednum,ledscr;
int mousedown[3]={0,0,0};
int mouseena=0,keyena=0;
int keystat=0xFF;
int keyrow,keycol;
unsigned char mousex,mousey;

int keyboardkeydown(int kr, int kc)
{
        if (!keystat)
        {
                rpclog("keydown\n");
//                printf(" %i %i\n",kr,kc);
                keyrow=kr;
                keycol=kc;
                sendkey(0xC0|keyrow);
                keystat=1;
                return 1;
        }
        return 0;
}

int keyboardkeyup(int kr, int kc)
{
        if (!keystat)
        {
                rpclog("keyup\n");
                keyrow=kr;
                keycol=kc;
                sendkey(0xD0|keyrow);
                keystat=2;
                return 1;
        }
        return 0;
}

FILE *klog;
void writekeyboard(unsigned char v)
{
        int c;
//        if (!klog) klog=fopen("keylog.txt","wt");
//        sprintf(s,"Write keyboard %02X %i %i %i\n",v,keystat,keyena,mouseena);
//        fputs(s,klog);
//        rpclog("Keyboard write %02X %i %08X\n",v,keystat,PC);
        switch (keystat)
        {
                case 0: /*Normal*/
//                rpclog("Normal - write %02X\n",v);
                switch (v&0xF0)
                {
                        case 0x00: /*Keyboard LEDs*/
                        ledcaps=v&1;
                        lednum=v&2;
                        ledscr=v&4;
                        break;
                        case 0x20: /*Keyboard ID*/
                        sendkey(0x81);
                        break;
                        case 0x30: /*Enable mouse, disable keyboard*/
//                        case 0x30: /*Enable mouse and keyboard*/
                        if (v&2) mouseena=1;
                        if (v&1) keyena=1;
                        break;
                        case 0xF0:
                        switch (v)
                        {
                                case 0xFF:  /*HRST*/
                                sendkey(0xFF); /*HRST*/
                                keystat=0xFF;
                                break;
                        }
                        break;
                }
                break;
                case 1: /*Second half of key down*/
//                rpclog("stat1 - write %02X\n",v);
                switch (v)
                {
                        case 0x3F: /*BACK*/
                        sendkey(0xC0|keycol);
                        keystat=0;
                        break;
                        default:
                        case 0xFF: /*HRST*/
                        sendkey(0xFF); /*HRST*/
                        keystat=0xFF;
                        break;
                }
                break;
                case 2: /*Second half of key up*/
//                rpclog("stat2 - write %02X\n",v);
                switch (v)
                {
                        case 0x3F: /*BACK*/
                        sendkey(0xD0|keycol);
                        keystat=0;
                        break;
                        default:
                        case 0xFF: /*HRST*/
                        sendkey(0xFF); /*HRST*/
                        keystat=0xFF;
                        break;
                }
                break;
                case 3: /*Second half of mouse*/
//                rpclog("stat3 - write %02X\n",v);
                switch (v)
                {
                        case 0x3F: /*BACK*/
                        sendkey(mousey);
                        keystat=0;
                        break;
                        default:
                        case 0xFF: /*HRST*/
                        sendkey(0xFF); /*HRST*/
                        keystat=0xFF;
                        break;
                }
                break;

                case 0xFF: /*Reset sequence*/
//                rpclog("Reset sequence - write %02X\n",v);
                switch (v)
                {
                        case 0xFF: /*HRST*/
                        sendkey(0xFF); /*HRST*/
                        for (c=0;c<128;c++) keydat[c]=0;
                        keyena=mouseena=0;
                        break;
                        case 0xFE: /*RAK1*/
                        sendkey(0xFE); /*RAK1*/
                        for (c=0;c<128;c++) keydat[c]=0;
                        keyena=mouseena=0;
                        break;
                        case 0xFD: /*RAK2*/
                        sendkey(0xFD); /*RAK2*/
                        keystat=0;
                        break;
                }
                break;
        }
}

void initkeyboard()
{
        int c,d;
        keystat=0xFF;
        sendkey(0xFF); /*HRST*/
        for (c=0;c<128;c++)
        {
                keytable[c][0]=keytable[c][1]=-1;
                keydat[c]=0;
        }
        c=d=0;
        while (!d)
        {
                keytable[keys[c][0]-1][0]=keys[c][1];
                keytable[keys[c][0]-1][1]=keys[c][2];
                c++;
                if (keys[c][0]==-1) d=1;
        }
}

FILE *klog;
void updatekeys()
{
        int mx,my;
        int c;
        unsigned char dx,dy;
        if (romset>3)
        {
                ioc.irqb|=0x20;
                updateirqs();
                return;
        }
//        rpclog("Updatekeys %i %i\n",keystat,keyena);
//        int mouseb=mouse_b;
//        mouse_b|=(key[KEY_MENU])?4:0;
        if (keystat) return;
        if (!keyena) return;
        for (c=1;c<128;c++)
        {
                if (key[c]!=keydat[c] && c!=KEY_MENU)
                {
                        if (key[c])
                        {
//                                        if (!klog) klog=fopen("key.log","wt");
//                                sprintf(s,"Key pressed %i %i %i\n",c-1,keytable[c-1][0],keytable[c-1][1]);
//                                fputs(s,klog);
                                if (keyboardkeydown(keytable[c-1][0],keytable[c-1][1]))
                                {
                                        keydat[c]=key[c];
                                        return;
                                }
                        }
                        else
                        {
                                if (keyboardkeyup(keytable[c-1][0],keytable[c-1][1]))
                                {
                                        keydat[c]=key[c];
                                        return;
                                }
                        }
                }
        }

        if ((mouse_b&1)!=mousedown[0]) /*Left button*/
        {
                if (mouse_b&1)
                {
                        if (keyboardkeydown(7,0))
                        {
                                mousedown[0]=1;
                                return;
                        }
                }
                else
                {
                        if (keyboardkeyup(7,0))
                        {
                                mousedown[0]=0;
                                return;
                        }
                }
        }
        if ((mouse_b&2)!=mousedown[1]) /*Right button*/
        {
                if (mouse_b&2)
                {
                        if (keyboardkeydown(7,2))
                        {
                                mousedown[1]=2;
                                return;
                        }
                }
                else
                {
                        if (keyboardkeyup(7,2))
                        {
                                mousedown[1]=0;
                                return;
                        }
                }
        }
        if (((mouse_b&4)|(key[KEY_MENU]?4:0))!=mousedown[2]) /*Middle button*/
        {
                if (((mouse_b&4)|(key[KEY_MENU]?4:0))&4)
                {
                        if (keyboardkeydown(7,1))
                        {
                                mousedown[2]=4;
                                return;
                        }
                }
                else
                {
                        if (keyboardkeyup(7,1))
                        {
                                mousedown[2]=0;
                                return;
                        }
                }
        }
//        printf("mouseena %i\n",mouseena);
        if (mouseena)// && (!mousehack || fullscreen))
        {
//                mx=mouse_x-moldx;
//                my=mouse_y-moldy;
//                moldx=mouse_x;
//                moldy=mouse_y;
                get_mouse_mickeys(&mx,&my);
                if (mousecapture && !fullscreen) position_mouse(320,256);
//                if (key[KEY_TILDE]) return;
                mx*=4;
                my*=4;
//                printf("Mouse %i %i  ",mx,my);
                if (!mx && !my) return;
                if (mx<0) dx=((-mx)>63)?63:-mx;
                else      dx=(mx>63)?63:mx;
                if (mx<0) dx=((dx^0x7F)+1)&0x7F;
                my=-my;
                if (my<0) dy=((-my)>63)?63:-my;
                else      dy=(my>63)?63:my;
                if (my<0) dy=((dy^0x7F)+1)&0x7F;
                mousex=dx;
                mousey=dy;
//                printf("%02X %02X\n",mousex,mousey);
                sendkey(mousex);
                keystat=3;
//                rpclog("Update mouse %i %i %i\n",mousex,mousey,keystat);
        }
}

void doosmouse()
{
        short temp;
        if (!mousehack || fullscreen) return;
        temp=1024-((mouse_y-offsety)<<1);
//        if (temp<0) temp=0;
        if (temp<mt) temp=mt;
        if (temp>mb) temp=mb;
//        ymouse=temp;
        writememl(0x5B8,temp);
        temp=(mouse_x-offsetx)<<1;
        if (temp>mr) temp=mr;
        if (temp<ml) temp=ml;
//        xmouse=temp;
        writememl(0x5B4,temp);
/*        *armregs[0]=mouse_x;
        if (mouse_x>639) *armregs[0]=639;
        *armregs[1]=mouse_y>>1;
        temp=0;
        if (mouse_b&1) temp|=1;
        if (mouse_b&2) temp|=4;
        if (mouse_b&4) temp|=2;
        if (key[KEY_MENU]) temp|=2;
        *armregs[2]=temp;
        *armregs[3]=0;*/
}

void setmousepos(unsigned long a)
{
        unsigned short temp,temp2;
        temp=readmemb(a+1)|(readmemb(a+2)<<8);
        temp=temp>>1;
        temp2=readmemb(a+3)|(readmemb(a+4)<<8);
        temp2=(1024-temp2)>>1;
//        position_mouse(temp,temp2);
}

void getunbufmouse(unsigned long a)
{
        short temp;
        temp=1024-((mouse_y-offsety)<<1);
        if (temp<mt) temp=mt;
        if (temp>mb) temp=mb;
        writememb(a+1,temp&0xFF);
        writememb(a+2,(temp>>8)&0xFF);
        temp=(mouse_x-offsetx)<<1;
        if (temp>mr) temp=mr;
        if (temp<ml) temp=ml;
        writememb(a+3,temp&0xFF);
        writememb(a+4,(temp>>8)&0xFF);
}

void getosmouse()
{
        long temp;
        temp=1024-((mouse_y-offsety)<<1);
        if (temp<mt) temp=mt;
        if (temp>mb) temp=mb;
        armregs[1]=temp;
        temp=(mouse_x-offsetx)<<1;
        if (temp>mr) temp=mr;
        if (temp<ml) temp=ml;
        armregs[0]=temp;
        temp=0;
        if (mouse_b&1) temp|=4;
        if (mouse_b&2) temp|=1;
        if (mouse_b&4) temp|=2;
        if (key[KEY_MENU]) temp|=2;
        armregs[2]=temp;
        armregs[3]=0;
}

void setmouseparams(unsigned long a)
{
        ml=readmemb(a+1)|(readmemb(a+2)<<8);
        mt=readmemb(a+3)|(readmemb(a+4)<<8);
        mr=readmemb(a+5)|(readmemb(a+6)<<8);
        mb=readmemb(a+7)|(readmemb(a+8)<<8);
//        sprintf(bigs,"Mouse params %04X %04X %04X %04X\n",ml,mr,mt,mb);
//        fputs(bigs,olog);
}

void resetmouse()
{
        ml=mt=0;
        mr=0x4FF;
        mb=0x3FF;
}
