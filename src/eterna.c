/*Arculator v0.8 by Tom Walker
  Eterna arcade emulation*/

#include <allegro.h>
#include "arc.h"

int oldkey5=0,oldkey6=0;
unsigned char readeterna(unsigned long addr)
{
        unsigned char temp=0;
        ioc.irqb&=~0x20;
        updateirqs();
        switch (addr&~3)
        {
                case 0x33C0004: /*DSW1*/
                return 0xE;//0xA;
                case 0x33C0008: /*DSW2*/
                return 0x30;
                case 0x3340010:
                case 0x33C0010: /*IN0*/
//                case 0x33C0008:
                if (key[KEY_5] && !oldkey5) temp|=0x10;
                if (key[KEY_6] && !oldkey6) temp|=0x08;
                if (key[KEY_1]) temp|=2;
                if (key[KEY_2]) temp|=1;
                break;
//                rpclog("Read IN0 %02X\n",temp);
//                return temp;
                case 0x33C0014: /*IN2*/
                return 0xFF;
                case 0x33C0018: /*IN1*/
                temp=0xFF;
                if (key[KEY_LCONTROL]) temp&=~0x01;
                if (key[KEY_RIGHT])    temp&=~0x02;
                if (key[KEY_LEFT])     temp&=~0x08;
                if (key[KEY_DOWN])     temp&=~0x10;
                if (key[KEY_UP])       temp&=~0x20;
                return temp;
        }
        oldkey5=key[KEY_5];
        oldkey6=key[KEY_6];
//        if (addr!=0x33400010) rpclog("Read Eterna %08X\n",addr);
        return temp;
//        return temp;
}

void writeeterna(unsigned long addr, unsigned long val)
{
}
