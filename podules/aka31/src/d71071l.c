#include <stdint.h>
#include <string.h>
#include "podules.h"
#include "aka31.h"
#include "d71071l.h"
#include "wd33c93a.h"

#define DMA_MODE_AUTOINIT (1 << 4)
#define DMA_MODE_DIR      (1 << 5)

void d71071l_init(d71071l_t *dma, podule_t *podule)
{
        memset(dma, 0, sizeof(d71071l_t));
        dma->podule = podule;
}

void d71071l_write(d71071l_t *dma, uint32_t addr, uint8_t val)
{
        switch (addr & 0x21c)
        {
                case 0x200:
                dma->base = val & 4;
                dma->selch = val & 3;
                break;
                
                case 0x004:
                dma->channel[dma->selch].count_base = (dma->channel[dma->selch].count_base & 0xff00) | val;
                if (!dma->base)
                        dma->channel[dma->selch].count_cur = (dma->channel[dma->selch].count_cur & 0xff00) | val;
                break;
                case 0x204:
                dma->channel[dma->selch].count_base = (dma->channel[dma->selch].count_base & 0xff) | (val << 8);
                if (!dma->base)
                        dma->channel[dma->selch].count_cur = (dma->channel[dma->selch].count_cur & 0xff) | (val << 8);
                break;

                case 0x008:
                dma->channel[dma->selch].addr_base = (dma->channel[dma->selch].addr_base & 0xffff00) | val;
                if (!dma->base)
                        dma->channel[dma->selch].addr_cur = (dma->channel[dma->selch].addr_cur & 0xffff00) | val;
                break;
                case 0x208:
                dma->channel[dma->selch].addr_base = (dma->channel[dma->selch].addr_base & 0xff00ff) | (val << 8);
                if (!dma->base)
                        dma->channel[dma->selch].addr_cur = (dma->channel[dma->selch].addr_cur & 0xff00ff) | (val << 8);
                break;
                case 0x00c:
                dma->channel[dma->selch].addr_base = (dma->channel[dma->selch].addr_base & 0x00ffff) | (val << 16);
                if (!dma->base)
                        dma->channel[dma->selch].addr_cur = (dma->channel[dma->selch].addr_cur & 0x00ffff) | (val << 16);
                break;
                
                case 0x014:
                dma->channel[dma->selch].mode = val;
                break;

                case 0x21c:
                dma->mask = val;
                break;
        }
}

uint8_t d71071l_read(d71071l_t *dma, uint32_t addr)
{
        switch (addr & 0x21c)
        {
                case 0x200:
                return dma->base | dma->selch;
                
                case 0x004:
                if (!dma->base)
                        return dma->channel[dma->selch].count_cur & 0xff;
                return dma->channel[dma->selch].count_base & 0xff;
                case 0x204:
                if (!dma->base)
                        return (dma->channel[dma->selch].count_cur >> 8) & 0xff;
                return (dma->channel[dma->selch].count_base >> 8) & 0xff;

                case 0x008:
                if (!dma->base)
                        return dma->channel[dma->selch].addr_cur & 0xff;
                return dma->channel[dma->selch].addr_base & 0xff;
                case 0x208:
                if (!dma->base)
                        return (dma->channel[dma->selch].addr_cur >> 8) & 0xff;
                return (dma->channel[dma->selch].addr_base >> 8) & 0xff;
                case 0x00c:
                if (!dma->base)
                        return (dma->channel[dma->selch].addr_cur >> 16) & 0xff;
                return (dma->channel[dma->selch].addr_base >> 16) & 0xff;
                
                case 0x014:
                return dma->channel[dma->selch].mode;

                case 0x21c:
                return dma->mask;
        }
	return 0;
}

int dma_read(d71071l_t *dma, int channel)
{
        uint8_t val;
        
        if (dma->mask & (1 << channel))
                return -1;
        if (!(dma->channel[channel].mode & (1 << 3)))
                return -1;
        val = aka31_read_ram(dma->podule, dma->channel[channel].addr_cur & 0xffff);
//aka31_log("dma_read: channel=%i addr=%04x count=%04x val=%02x\n", channel, dma->channel[channel].addr_cur, dma->channel[channel].count_cur, val);
        if (dma->channel[channel].mode & DMA_MODE_DIR)
                dma->channel[channel].addr_cur--;
        else
                dma->channel[channel].addr_cur++;
        
        dma->channel[channel].count_cur--;
        
        if (dma->channel[channel].count_cur < 0)
        {
                if (dma->channel[channel].mode & DMA_MODE_AUTOINIT)
                {
                        dma->channel[channel].addr_cur  = dma->channel[channel].addr_base;
                        dma->channel[channel].count_cur = dma->channel[channel].count_base;
                }
                else
                {
                        aka31_tc_int(dma->podule);
                        dma->mask |= (1 << channel);
//                        aka31_log("dma_tc\n");
                }
        }
        
        return val;
}

int dma_write(d71071l_t *dma, int channel, uint8_t val)
{
        if (dma->mask & (1 << channel))
                return -1;
        if (dma->channel[channel].mode & (1 << 3))
                return -1;
//aka31_log("dma_write: channel=%i val=%02x addr=%04x count=%04x\n", channel, val, dma->channel[channel].addr_cur, dma->channel[channel].count_cur);
        aka31_write_ram(dma->podule, dma->channel[channel].addr_cur & 0xffff, val);
        if (dma->channel[channel].mode & DMA_MODE_DIR)
                dma->channel[channel].addr_cur--;
        else
                dma->channel[channel].addr_cur++;
        
        dma->channel[channel].count_cur--;
        
        if (dma->channel[channel].count_cur < 0)
        {
                if (dma->channel[channel].mode & DMA_MODE_AUTOINIT)
                {
                        dma->channel[channel].addr_cur  = dma->channel[channel].addr_base;
                        dma->channel[channel].count_cur = dma->channel[channel].count_base;
                }
                else
                {
                        aka31_tc_int(dma->podule);
                        dma->mask |= (1 << channel);
//                        aka31_log("dma_tc\n");
                }
        }
        
        return 0;
}

