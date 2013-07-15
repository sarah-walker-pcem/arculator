#include <stdio.h>
#include "arc.h"

uint8_t icsrom[16384];
int icspage=0;

void resetics()
{
        FILE *f=fopen("idefs","rb");
        if (f)
        {
                fread(icsrom,16384,1,f);
                fclose(f);
        }
        icspage=0;
}

void writeics(uint32_t addr, uint8_t val)
{
//        rpclog("Write ICS %07X %02X %07X\n",addr,val,PC);
        switch (addr&0x3000)
        {
                case 0x2000: icspage=val; return;
        }
}

uint8_t readics(uint32_t addr)
{
        int temp;
        return 0xFF;
        if (romset<2 || romset>3) return 0xFF;
//        rpclog("Read ICS %07X %07X\n",addr,PC);
        switch (addr&0x3000)
        {
                case 0x0000: case 0x1000:
                temp=((addr&0x1FFC)|(icspage<<13))>>2;
//                rpclog("Read IROM %04X %i %04X %02X\n",addr,icspage,temp,icsrom[temp&0x3FFF]);
                return icsrom[temp&0x3FFF];
        }
}
