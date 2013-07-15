#include <stdio.h>
#include "arc.h"

uint8_t arcrom[32768];
int arcpage=0;

void resetarcrom()
{
        FILE *f=fopen("arcrom","rb");
        if (f)
        {
                fread(arcrom,32768,1,f);
                fclose(f);
        }
        arcpage=0;
}

void writearcrom(uint32_t addr, uint8_t val)
{
//        rpclog("Write ARC %07X %02X %07X\n",addr,val,PC);
        switch (addr&0x3000)
        {
                case 0x2000: arcpage=val; return;
        }
}

uint8_t readarcrom(uint32_t addr)
{
        int temp;
        if (romset<2 || romset>3) return 0xFF;
//        rpclog("Read ICS %07X %07X\n",addr,PC);
        switch (addr&0x3000)
        {
                case 0x0000: case 0x1000:
                temp=((addr&0x1FFC)|(arcpage<<13))>>2;
//                rpclog("Read AROM %04X %i %04X %02X\n",addr,arcpage,temp,arcrom[temp&0x7FFF]);
                return arcrom[temp&0x7FFF];
        }
}
