/*Arculator 2.0 by Sarah Walker
  Keyboard/mouse emulation*/
#include <stdio.h>
#include "arc.h"
#include "ioc.h"
#include "keyboard.h"
#include "plat_input.h"
#include "keytable.h"
#include "timer.h"

static emu_timer_t keyboard_timer;
static emu_timer_t keyboard_rx_timer;
static emu_timer_t keyboard_tx_timer;
static int mouse_b, mouse_x, mouse_y;

int mousecapture=0;
int ml,mr,mt,mb;

int keydat[512];

int moldx,moldy;
int ledcaps,lednum,ledscr;
int mousedown[3]={0,0,0};
int mouseena=0,keyena=0;
int keystat=0xFF;
int keyrow,keycol;
uint8_t mousex,mousey;

static uint8_t key_data[2];

enum
{
        KEYBOARD_HRST = 0xff,
        KEYBOARD_RAK1 = 0xfe,
        KEYBOARD_RAK2 = 0xfd,
        
        KEYBOARD_BACK = 0x3f
};

enum
{
        KEYBOARD_RESET = 0,
        KEYBOARD_IDLE = 1,
        KEYBOARD_DAT1 = 2,
        KEYBOARD_DAT2 = 3,
        KEYBOARD_DAT_SINGLE = 4
};

void keyboard_send_single(uint8_t val)
{
        LOG_KB_MOUSE("keyboard_send_single %02X\n", val);
        key_data[0] = val;
        
        keystat = KEYBOARD_DAT_SINGLE;
        
        timer_set_delay_u64(&keyboard_rx_timer, 1000 * TIMER_USEC);
}

void keyboard_send_double(uint8_t val1, uint8_t val2)
{
        LOG_KB_MOUSE("keyboard_send_double : %02X %02X\n", val1, val2);
        key_data[0] = val1;        
        key_data[1] = val2;
        
        keystat = KEYBOARD_DAT1;
        
        timer_set_delay_u64(&keyboard_rx_timer, 1000 * TIMER_USEC);
}

void key_do_rx_callback()
{
        LOG_KB_MOUSE("RX interrupt\n");
        ioc_irqb(IOC_IRQB_KEYBOARD_RX);
}

void key_do_tx_callback()
{
        LOG_KB_MOUSE("TX interrupt\n");
        ioc_irqb(IOC_IRQB_KEYBOARD_TX);
}

int keyboard_keydown(int row, int col)
{
        if (!keystat)
        {
                LOG_KB_MOUSE("keydown\n");
                keyboard_send_double(0xc0 | row, 0xc0 | col);
                return 1;
        }
        return 0;
}

int keyboard_keyup(int row, int col)
{
        if (!keystat)
        {
                LOG_KB_MOUSE("keyup\n");
                keyboard_send_double(0xd0 | row, 0xd0 | col);
                return 1;
        }
        return 0;
}

uint8_t keyboard_read()
{
        switch (keystat)
        {
                case KEYBOARD_RESET:
                case KEYBOARD_IDLE:
                return key_data[0];
                
                case KEYBOARD_DAT_SINGLE:
                keystat = KEYBOARD_IDLE;
                case KEYBOARD_DAT1:
                return key_data[0];
                case KEYBOARD_DAT2:
                return key_data[1];
        }
        
        return 0;
}

void keyboard_write(uint8_t val)
{
        int c;

        timer_set_delay_u64(&keyboard_tx_timer, 1000 * TIMER_USEC);

        LOG_KB_MOUSE("Keyboard write %02X %i %08X\n", val, keystat, PC);
        switch (keystat)
        {
                case KEYBOARD_RESET: /*Reset sequence*/
                LOG_KB_MOUSE("Reset sequence - write %02X\n", val);
                switch (val)
                {
                        case KEYBOARD_HRST: /*HRST*/
                        keyboard_send_single(KEYBOARD_HRST); /*HRST*/
                        for (c = 0; c < 512; c++) 
                                keydat[c] = 0;
                        keyena = mouseena = 0;
                        keystat = KEYBOARD_RESET;
                        break;
                        case KEYBOARD_RAK1: /*RAK1*/
                        keyboard_send_single(KEYBOARD_RAK1); /*RAK1*/
                        for (c = 0; c < 512; c++) 
                                keydat[c] = 0;
                        keyena = mouseena = 0;
                        keystat = KEYBOARD_RESET;
                        break;
                        case KEYBOARD_RAK2: /*RAK2*/
                        keyboard_send_single(KEYBOARD_RAK2); /*RAK2*/
                        keystat = KEYBOARD_IDLE;
                        break;
                }
                break;

                case KEYBOARD_IDLE: /*Normal*/
//                LOG_KB_MOUSE("Normal - write %02X\n",v);
                switch (val)
                {
                        case 0x00: case 0x01: case 0x02: case 0x03: /*Keyboard LEDs*/
                        case 0x04: case 0x05: case 0x06: case 0x07:
                        ledcaps = val & 1;
                        lednum  = val & 2;
                        ledscr  = val & 4;
                        break;
                        case 0x20: /*Keyboard ID*/
                        keyboard_send_single(0x81);
                        break;
                        case 0x30: case 0x31: case 0x32: case 0x33: /*Enable mouse, disable keyboard*/
                        if (val & 2) mouseena = 1;
                        if (val & 1) keyena = 1;
                        break;
                        case KEYBOARD_HRST:  /*HRST*/
                        keyboard_send_single(KEYBOARD_HRST);
                        keystat = KEYBOARD_RESET;
                        break;
                }
                break;
                
                case KEYBOARD_DAT1:
                switch (val)
                {
                        case KEYBOARD_BACK:
                        keystat = KEYBOARD_DAT2;
                        timer_set_delay_u64(&keyboard_rx_timer, 1000 * TIMER_USEC);
                        break;
                        
                        case KEYBOARD_HRST:
                        keyboard_send_single(KEYBOARD_HRST);
                        keystat = KEYBOARD_RESET;
                        break;
                }
                break;

                case KEYBOARD_DAT2:
                case KEYBOARD_DAT_SINGLE:
                switch (val)
                {
                        case 0x30: case 0x31: case 0x32: case 0x33: /*Enable mouse, disable keyboard*/
                        if (val & 2) mouseena = 1;
                        if (val & 1) keyena = 1;
                        keystat = KEYBOARD_IDLE;
                        break;
                        
                        case KEYBOARD_HRST:
                        keyboard_send_single(KEYBOARD_HRST);
                        keystat = KEYBOARD_RESET;
                        break;
                }
                break;
        }
}

void keyboard_init()
{
        int c, d;
        
        keyboard_send_single(KEYBOARD_HRST);
        keystat = KEYBOARD_RESET;        
        for (c = 0; c < 512; c++)
        {
                keytable[c][0] = keytable[c][1] = -1;
                keydat[c] = 0;
        }
        c = d = 0;
        while (!d)
        {
                keytable[keys[c][0] - 1][0] = keys[c][1];
                keytable[keys[c][0] - 1][1] = keys[c][2];
                c++;
                if (keys[c][0] == -1)
                        d = 1;
        }
        
        timer_add(&keyboard_timer, keyboard_poll, NULL, 1);
        timer_add(&keyboard_rx_timer, key_do_rx_callback, NULL, 0);
        timer_add(&keyboard_tx_timer, key_do_tx_callback, NULL, 0);
}

FILE *klog;
void keyboard_poll(void *p)
{
        int mx,my;
        int mouse_buttons;
        int c;
        uint8_t dx, dy;
        
        timer_advance_u64(&keyboard_timer, TIMER_USEC * 10000);
        
        /*if (romset > 3)
        {
                ioc_irqb(IOC_IRQB_PODULE_IRQ);
                return;
        }*/
//        LOG_KB_MOUSE("Updatekeys %i %i\n",keystat,keyena);
//        int mouseb=mouse_b;
//        mouse_b|=(key[KEY_MENU])?4:0;

        if (keystat || keyena) LOG_KB_MOUSE("keyboard_poll %i %i\n", keystat, keyena);
        if (keystat != KEYBOARD_IDLE)
                return;
        if (!keyena) 
                return;
//        c = poll_keyboard();
        for (c = 1; c < 512; c++)
        {
                if (key[c] != keydat[c] && c != KEY_MENU && keytable[c-1][0] != -1)
                {
                        if (key[c])
                        {
                                keyboard_send_double(0xc0 | keytable[c-1][0], 0xc0 | keytable[c-1][1]);
                        }
                        else
                        {
                                keyboard_send_double(0xd0 | keytable[c-1][0], 0xd0 | keytable[c-1][1]);
                        }
                        keydat[c] = key[c];
                        return;
                }
        }

        mouse_buttons = /*mousecapture ? */mouse_get_buttons()/* : 0*/;
        
        if ((mouse_buttons & 1) != mousedown[0]) /*Left button*/
        {
                LOG_KB_MOUSE("mouse left click\n");
                mousedown[0] = mouse_buttons & 1;
                if (mousedown[0])
                        keyboard_send_double(0xc7, 0xc0);
                else
                        keyboard_send_double(0xd7, 0xd0);
                return;
        }
        if ((mouse_buttons & 2) != mousedown[1]) /*Right button*/
        {
                LOG_KB_MOUSE("mouse right click\n");
                mousedown[1] = mouse_buttons & 2;
                if (mousedown[1])
                        keyboard_send_double(0xc7, 0xc2);
                else
                        keyboard_send_double(0xd7, 0xd2);
                return;
        }
        /* There are three ways to perform a menu-click in Arculator:
           - Click the middle mouse button, on a three-button mouse
           - Press the Menu key (KEY_MENU), on a modern Windows keyboard
           - Press the left Command key (KEY_LWIN), on a Mac*/
        if (((mouse_buttons & 4) | ((key[KEY_MENU] || key[KEY_LWIN]) ? 4 : 0)) != mousedown[2]) /*Middle button*/
        {
                LOG_KB_MOUSE("mouse middle click / menu key pressed\n");
                mousedown[2] = (mouse_buttons & 4) | ((key[KEY_MENU] || key[KEY_LWIN]) ? 4 : 0);
                if (mousedown[2])
                        keyboard_send_double(0xc7, 0xc1);
                else
                        keyboard_send_double(0xd7, 0xd1);
                return;
        }

        if (mouseena && !mousehack/* && mousecapture*/)// && (!mousehack || fullscreen))
        {
                mouse_get_mickeys(&mx,&my);
//                if (mousecapture && !fullscreen) 
//                        position_mouse(320,256);

                if (!mx && !my) 
                        return;
                        
                if (mx < 0) dx = ((-mx) > 63) ? 63 : -mx;
                else        dx = (mx > 63) ? 63 : mx;
                if (mx < 0) dx = ((dx ^ 0x7f) + 1) & 0x7f;
                my = -my;
                if (my < 0) dy = ((-my) > 63) ? 63 : -my;
                else        dy = (my > 63) ? 63 : my;
                if (my < 0) dy = ((dy ^ 0x7f) + 1) & 0x7f;
                mousex = dx;
                mousey = dy;

                keyboard_send_double(mousex, mousey);

                LOG_KB_MOUSE("Update mouse %i %i %i\n", mousex, mousey, keystat);
        }
}

void doosmouse()
{
        short temp;
        if (!mousehack || fullscreen) return;
        LOG_KB_MOUSE("doosmouse\n");
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

void setmousepos(uint32_t a)
{
        uint16_t temp,temp2;
        LOG_KB_MOUSE("setmousepos\n");
        temp=readmemb(a+1)|(readmemb(a+2)<<8);
        temp=temp>>1;
        temp2=readmemb(a+3)|(readmemb(a+4)<<8);
        temp2=(1024-temp2)>>1;
//        position_mouse(temp,temp2);
}

void getunbufmouse(uint32_t a)
{
        short temp;
        LOG_KB_MOUSE("getunbufmouse\n");
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
        LOG_KB_MOUSE("getosmouse\n");
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
        LOG_KB_MOUSE("%08X %08X %08X\n",armregs[0],armregs[1],armregs[2]);
}

void setmouseparams(uint32_t a)
{
        LOG_KB_MOUSE("setmouseparams\n");
        ml=readmemb(a+1)|(readmemb(a+2)<<8);
        mt=readmemb(a+3)|(readmemb(a+4)<<8);
        mr=readmemb(a+5)|(readmemb(a+6)<<8);
        mb=readmemb(a+7)|(readmemb(a+8)<<8);
}

void resetmouse()
{
        ml=mt=0;
        mr=0x4FF;
        mb=0x3FF;
}
