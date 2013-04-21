/*Arculator 0.8 by Tom Walker
  ARM3 CP15 emulation*/
  
#include "arc.h"

struct
{
        unsigned long ctrl;
        unsigned long cache,update,disrupt;
} arm3cp;

void resetcp15()
{
        arm3cp.ctrl=0;
}

unsigned long readcp15(int reg)
{
        switch (reg)
        {
                case 0: /*ID*/
                return 0x41560300; /*VLSI ARM3*/
                case 2: /*CTRL*/
                return arm3cp.ctrl;
                case 3: /*Cacheable areas*/
                return arm3cp.cache;
                case 4: /*Updateable areas*/
                return arm3cp.update;
                case 5: /*Disruptive areas*/
                return arm3cp.disrupt;
        }
        return 0;
}

void writecp15(int reg, unsigned long val)
{
        switch (reg)
        {
                case 2: /*CTRL*/
                arm3cp.ctrl=val;
                if (val&1) speed=(arm3==3)?3:2;
                else       speed=1;
                redovideotiming();
//                redoioctiming();
                return;
                case 3: /*Cacheable areas*/
                arm3cp.cache=val;
                return;
                case 4: /*Updateable areas*/
                arm3cp.update=val;
                return;
                case 5: /*Disruptive areas*/
                arm3cp.disrupt=val;
                return;
        }
}

