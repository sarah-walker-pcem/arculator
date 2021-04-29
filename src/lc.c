#include "arc.h"
#include "lc.h"

uint8_t lc_read(uint32_t addr)
{
        uint8_t ret = 0xff;

        switch (addr & 0x3c)
        {
                case 0x3c:
                ret = 4;
                break;
        }

//        rpclog("lc_read: addr=%07x val=%02x PC=%07x\n", addr, ret, PC);

        return ret;
}

void lc_write(uint32_t addr, uint8_t val)
{
//        rpclog("lc_write: addr=%07x val=%02x PC=%07x\n", addr, val, PC);
}
