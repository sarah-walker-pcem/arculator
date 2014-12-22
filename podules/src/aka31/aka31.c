#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "podules-win.h"
#include "aka31.h"
#include "wd33c93a.h"

extern __declspec(dllexport) int InitDll();
extern __declspec(dllexport) uint8_t  readb(podule *p, int easi, uint32_t addr);
extern __declspec(dllexport) uint16_t readw(podule *p, int easi, uint32_t addr);
//extern __declspec(dllexport) uint32_t readl(podule *p, int easi, uint32_t addr);
extern __declspec(dllexport) void writeb(podule *p, int easi, uint32_t addr, uint8_t val);
extern __declspec(dllexport) void writew(podule *p, int easi, uint32_t addr, uint16_t val);
//extern __declspec(dllexport) void writel(podule *p, int easi, uint32_t addr, uint32_t val);

extern __declspec(dllexport) void memc_writew(podule *p, uint32_t addr, uint16_t val);
extern __declspec(dllexport) void memc_writeb(podule *p, uint32_t addr, uint8_t  val);
extern __declspec(dllexport) uint16_t memc_readw(podule *p, uint32_t addr);
extern __declspec(dllexport) uint8_t  memc_readb(podule *p, uint32_t addr);

extern __declspec(dllexport) void reset(podule *p);
extern __declspec(dllexport) int timercallback(podule *p);

void aka31_update_ints(podule *p);

#define AKA31_POD_IRQ  0x01
#define AKA31_TC_IRQ   0x02
#define AKA31_SBIC_IRQ 0x08

#define AKA31_PAGE_MASK   0x3f
#define AKA31_ENABLE_INTS 0x40
#define AKA31_RESET       0x80

static uint8_t aka31_rom[0x10000];
uint8_t aka31_ram[0x10000];
static int aka31_page;
static FILE *aka31_logf;
static uint8_t aka31_intstat;

void aka31_log(const char *format, ...)
{
   	char buf[1024];
   	va_list ap;
   	
	return;
   	
	if (!aka31_logf)
		aka31_logf = fopen("aka31_log.txt", "wt");

   	va_start(ap, format);
   	vsprintf(buf, format, ap);
   	va_end(ap);
   	fputs(buf, aka31_logf);
   	fflush(aka31_logf);
}

uint8_t readb(podule *p, int easi, uint32_t addr)
{
        int temp;

        aka31_log("Read aka31 B %04X\n", addr);
                
        switch (addr & 0x3000)
        {
                case 0x0000: case 0x1000:
                temp = ((addr & 0x1ffc) | ((aka31_page & AKA31_PAGE_MASK) << 13)) >> 2;
                //aka31_log("  ROM %05X %02X\n", temp, aka31_rom[temp]);
                return aka31_rom[temp];

		case 0x2000:                
                aka31_log("Read intstat %02x\n", aka31_intstat);
                return aka31_intstat;

                default:
                aka31_log("Read aka31 %04X\n", addr);
                return 0xff;
        }

        return 0xff;
}

uint16_t readw(podule *p, int easi, uint32_t addr)
{
        aka31_log("Read aka31 W %04X\n", addr);
        return readb(p, easi, addr);
}

void writeb(podule *p, int easi, uint32_t addr, uint8_t val)
{     
        aka31_log("Write aka31 B %04X %02X\n", addr, val);
        switch (addr & 0x3000)
        {
                case 0x2000:
                aka31_log("Write intclear\n");
                aka31_intstat &= ~AKA31_TC_IRQ;
                if (!(aka31_intstat & AKA31_SBIC_IRQ))
                {
                        aka31_intstat = 0;
                        p->irq = 0;
                }
                break;
                case 0x3000:
                aka31_log("Write page %02x\n", val);
		if (!(val & AKA31_RESET) && (aka31_page & AKA31_RESET))
			wd33c93a_reset(p);
                aka31_page = val; 
		aka31_update_ints(p);
                return;

                default:
                aka31_log("Write aka31 %04X %02X\n", addr, val);
        }
}

void writew(podule *p, int easi, uint32_t addr, uint16_t val)
{
        aka31_log("Write aka31 W %04X %02X\n", addr, val);
        writeb(p, easi, addr, val);
}

uint8_t memc_readb(podule *p, uint32_t addr)
{
        int temp;

        aka31_log("Read aka31 MEMC B %04X %i\n", addr, p->msectimer);

        if (!(addr & 0x2000))
        {
                temp = ((addr & 0x1ffe) | ((aka31_page & AKA31_PAGE_MASK) << 13)) >> 1;
                aka31_log("Read aka31 MEMC B %04X %04x %02x\n", addr, temp, aka31_ram[temp | (addr & 1)]);
                return aka31_ram[temp | (addr & 1)];
        }

	if (aka31_page & AKA31_RESET)
		return 0xff;
                
        switch (addr & 0x3000)
        {
                case 0x2000:
                return wd33c93a_read(addr, p);
                
                case 0x3000:
                return d71071l_read(addr, p);

                default:
                aka31_log("Read aka31 %04X\n", addr);
                return 0xff;
        }

        return 0xff;
}

uint16_t memc_readw(podule *p, uint32_t addr)
{
        int temp;
        if (!(addr & 0x2000))
        {
                temp = ((addr & 0x1ffe) | ((aka31_page & AKA31_PAGE_MASK) << 13)) >> 1;
//                aka31_log("Read aka31 MEMC W %04X %04x %04x\n", addr, temp, aka31_ram[temp] | (aka31_ram[temp+1] << 8));
                return aka31_ram[temp] | (aka31_ram[temp+1] << 8);
        }
        return memc_readb(p, addr);
}

void memc_writeb(podule *p, uint32_t addr, uint8_t val)
{     
        aka31_log("Write aka31 MEMC B %04X %02X\n", addr, val);

        if (!(addr & 0x2000))
        {
                int temp = ((addr & 0x1ffe) | ((aka31_page & AKA31_PAGE_MASK) << 13)) >> 1;
                aka31_log(" Write to RAM %04x\n", temp);
                aka31_ram[temp | (addr & 1)] = val;
                return;
        }

	if (aka31_page & AKA31_RESET)
		return;

        switch (addr & 0x3000)
        {
                case 0x2000:
                wd33c93a_write(addr, val, p);
                break;

                case 0x3000:
                d71071l_write(addr, val, p);
                break;

                default:
                aka31_log("Write aka31 %04X %02X\n", addr, val);
        }
}
void memc_writew(podule *p, uint32_t addr, uint16_t val)
{
        int temp;
        aka31_log("Write aka31 MEMC W %04X %02X\n", addr, val);
        if (!(addr & 0x2000))
        {
                temp = ((addr & 0x1ffe) | ((aka31_page & AKA31_PAGE_MASK) << 13)) >> 1;
                aka31_log(" Write to RAM %04x\n", temp);
                aka31_ram[temp] = val & 0xff;
                aka31_ram[temp+1] = (val >> 8);
                return;
        }
        memc_writeb(p, addr, val);
}

void reset(podule *p)
{
        aka31_log("Reset aka31\n");
        p->msectimer = 5;
        aka31_page = 0;
}

static void aka31_init()
{
        FILE *f;
//        append_filename(fn,exname,"zidefs",sizeof(fn));
        f = fopen("scsirom", "rb");
        if (!f)
        {
                aka31_log("Failed to open SCSIROM!\n");
                return;
        }
        fread(aka31_rom, 0x10000, 1, f);
        fclose(f);
        
        aka31_page = 0;
        wd33c93a_init();
//        addpodule(NULL,icswritew,icswriteb,NULL,icsreadw,icsreadb,NULL);
        aka31_log("aka31 Initialised!\n");
}

int timercallback(podule *p)
{
        aka31_log("callback %i\n", p->msectimer);
        wd33c93a_poll(p);
        return 5;
}

void aka31_update_ints(podule *p)
{
	if (aka31_intstat && (aka31_page & AKA31_ENABLE_INTS))
		p->irq = 1;
	else
		p->irq = 0;
}

void aka31_sbic_int(podule *p)
{
        aka31_intstat |= AKA31_SBIC_IRQ | AKA31_POD_IRQ;
        if (aka31_page & AKA31_ENABLE_INTS)
                p->irq = 1;
}
void aka31_sbic_int_clear(podule *p)
{
        aka31_log("aka31_sbic_int_clear\n");
        aka31_intstat &= ~AKA31_SBIC_IRQ;
        if (!(aka31_intstat & AKA31_TC_IRQ))
        {
                aka31_intstat = 0;
                p->irq = 0;
        }
}
void aka31_tc_int(podule *p)
{
        aka31_intstat |= AKA31_TC_IRQ | AKA31_POD_IRQ;
        if (aka31_page & AKA31_ENABLE_INTS)
                p->irq = 1;
}

int InitDll()
{
        aka31_log("InitDll\n");
        aka31_init();
}

BOOL APIENTRY DllMain (HINSTANCE hInst     /* Library instance handle. */ ,
                       DWORD reason        /* Reason this function is being called. */ ,
                       LPVOID reserved     /* Not used. */ )
{
        aka31_log("DllMain\n");
    switch (reason)
    {
      case DLL_PROCESS_ATTACH:
        break;

      case DLL_PROCESS_DETACH:
        break;

      case DLL_THREAD_ATTACH:
        break;

      case DLL_THREAD_DETACH:
        break;
    }

    /* Returns TRUE on success, FALSE on failure */
    return TRUE;
}
