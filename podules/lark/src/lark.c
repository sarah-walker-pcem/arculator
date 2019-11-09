/*Lark memory map :

  IOC:
  0000        : XC3030 FPGA (write)
                bit 0 - DIN
                bit 1 - CCLK
                bit 2 - INIT?  bits 2 & 3 may be reversed
                bit 3 - RESET?
  0000 - 1fff : banked ROM (read)
  2000        : ROM page register
  3000        : FPGA? 
        3000 read : IRQ status (/clear?)
                bit 0 - IRQ master
                bit 1 - AD1848 IRQ
                bit 2 - 16550 IRQ
                bit 6 - output FIFO half empty
                bit 7 - ~input FIFO half full
        3000 write :
                bit 3 - enable output FIFO
                bit 4 - enable input FIFO?
                bit 7 - reset?
  3400        : 16550 serial
  3c00 - 3c0f : AD1848 CODEC

  MEMC:
  0000 : Write to output FIFO
  0000 : Read from input FIFO
  

  Basic design :

  Lark uses an AD1848 (WSS-compatible) CODEC connected to two AM7202A 1024x9
  FIFOs - one for input, one for output. The FIFOs are 1024x9, these are used as
  512x16 with the FPGA converting one 16-bit MEMC access to 2 8-bit FIFO
  accesses (and vice versa).
  
  The Lark driver doesn't seem to use the AD1848 IRQ, just clearing the IRQ
  state. Instead the FIFO half-full flags are connected to interrupt lines and
  are used to trigger CPU transfers to and from the FIFOs.
  
  The FPGA seems to implement a single read and single write port at 0x3000.
  Read is IRQ status, writing acts as control. Control seems to include enable
  bits for AD1848 DMA transfers to/from the FIFOs, and also a reset bit? There
  are several other bits that I haven't figured out the meaning of yet.
  
  Internal Arc speaker is connected to one of the aux inputs, I am not sure
  which one. LK13 is connected to the other.
  
  MIDI is just handled by a standard 16550, same as the MIDI Max and many other
  MIDI podules.
*/

#ifdef WIN32
#include <windows.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "podule_api.h"
#include "16550.h"
#include "ad1848.h"
#include "am7202a.h"
#include "lark.h"
#include "midi.h"
#include "sound_in.h"
#include "sound_out.h"

#ifdef WIN32
extern __declspec(dllexport) const podule_header_t *podule_probe(const podule_callbacks_t *callbacks, char *path);
#else
#define BOOL int
#define APIENTRY
#endif

static const podule_callbacks_t *podule_callbacks;
char podule_path[512];

#define MIDI_UART_CLOCK 2000000 //(31250Hz * 4 * 16)

#define CTRL_OUT_FIFO_ENA (1 << 3)
#define CTRL_IN_FIFO_ENA (1 << 4)

typedef struct lark_t
{
        uint8_t rom[0x20000];
        int page;
        uint8_t irqstat;
        uint8_t ctrl;
        
        ad1848_t ad1848;
        am7202a_t out_fifo, in_fifo;
        n16550_t n16550;
        
        void *sound_in;
        void *sound_out;
        void *midi;
        
        podule_t *podule;
} lark_t;

#ifdef DEBUG_LOG
static FILE *lark_logf;
#endif
void lark_log(const char *format, ...)
{
#ifdef DEBUG_LOG
   char buf[1024];

   if (!lark_logf)
        lark_logf = fopen("lark_log.txt", "wt");
   va_list ap;
   va_start(ap, format);
   vsprintf(buf, format, ap);
   va_end(ap);
   fputs(buf, lark_logf);
   fflush(lark_logf);
#endif
}

static void lark_update_irqs(lark_t *lark)
{
        uint8_t irq_status = lark->irqstat ^ 0x80;

        if (!(lark->ctrl & 0x10))
                irq_status &= 0x7f;

        if (irq_status)
                podule_callbacks->set_irq(lark->podule, 1);
        else
                podule_callbacks->set_irq(lark->podule, 0);
}

static uint8_t lark_read_b(struct podule_t *podule, podule_io_type type, uint32_t addr)
{
        lark_t *lark = podule->p;
        uint8_t temp = 0xff;

        if (type != PODULE_IO_TYPE_IOC)
        {
                lark_log("lark_read_b: MEMC %04x\n", addr);
                return 0xff;
        }

        switch (addr&0x3c00)
        {
                case 0x0000: case 0x0400: case 0x0800: case 0x0c00:
                case 0x1000: case 0x1400: case 0x1800: case 0x1c00:
                addr = ((addr & 0x1ffc) | (lark->page << 13)) >> 2;
                //lark_log("  ROM %05X %02X\n", temp, lark_rom[temp]);
                temp = lark->rom[addr];
                break;

                case 0x3000:
                temp = lark->irqstat;
//                lark->irqstat = 0;
//                podule_callbacks->set_irq(podule, 0);
                if (temp ^ 0x80)
                        temp |= 0x01;
//                lark_log("Return IRQstat %02X\n", temp);
                break;

                case 0x3400:
                temp = n16550_read(&lark->n16550, (addr >> 2) & 7);
                break;

                case 0x3c00:
                temp = ad1848_read(&lark->ad1848, (addr >> 2) | 4);
                break;
        }
//        lark_log("lark_read_b: IOC %04x %02x\n", addr, temp);
                
        return temp;
}

static void lark_write_b(struct podule_t *podule, podule_io_type type, uint32_t addr, uint8_t val)
{        
        lark_t *lark = podule->p;

        if (type != PODULE_IO_TYPE_IOC)
        {
                lark_log("lark_write_b: MEMC %04x %02x\n", addr, val);
                return;
        }

        switch (addr & 0x3c00)
        {
                case 0x2000: 
                lark->page = val;
                break;
                
                case 0x3000:
                lark->ctrl = val;
                lark_update_irqs(lark);
//                lark_log("lark write control %02x\n", val);
                if (val & 0x80)
                        am7202a_reset(&lark->out_fifo);
                break;
                
                case 0x3400:
                n16550_write(&lark->n16550, (addr >> 2) & 7, val);
                break;

                case 0x3c00:
                ad1848_write(&lark->ad1848, (addr >> 2) | 4, val);
                break;
        }
//        lark_log("lark_write_b: IOC %04x %02x\n", addr, val);
}

static uint16_t lark_read_w(struct podule_t *podule, podule_io_type type, uint32_t addr)
{
        lark_t *lark = podule->p;
        uint16_t temp = 0xffff;
        
        if (type == PODULE_IO_TYPE_IOC)
                lark_log("lark_read_w: IOC %04x\n", addr);
        else
        {
//                lark_log("lark_read_w: MEMC %04x\n", addr);
                
                temp = am7202a_read(&lark->in_fifo);
                temp |= am7202a_read(&lark->in_fifo) << 8;
        }
                
        return temp;
}

static void lark_write_w(struct podule_t *podule, podule_io_type type, uint32_t addr, uint16_t val)
{
        lark_t *lark = podule->p;

        if (type == PODULE_IO_TYPE_IOC)
                lark_log("lark_write_w: IOC %04x %04x\n", addr, val);
        else
        {
//                lark_log("lark_write_w: MEMC %04x %04x\n", addr, val);
                am7202a_write(&lark->out_fifo, val & 0xff);
                am7202a_write(&lark->out_fifo, val >> 8);
        }
}

static int lark_run(struct podule_t *podule, int timeslice_us)
{
        lark_t *lark = podule->p;
        
        ad1848_run(&lark->ad1848, timeslice_us);
        n16550_run(&lark->n16550, timeslice_us);

        return 320; /*320us - 1 8N1 byte at 31250Hz (MIDI transfer speed)*/
}

void lark_set_irq(lark_t *lark, uint8_t irq)
{
        lark->irqstat |= irq;
        lark_update_irqs(lark);
}
void lark_clear_irq(lark_t *lark, uint8_t irq)
{
        lark->irqstat &= ~irq;
        lark_update_irqs(lark);
}

void lark_sound_out_buffer(struct lark_t *lark, void *buffer, int samples)
{
        sound_out_buffer(lark->sound_out, buffer, samples);
}

static void lark_uart_irq(void *p, int state)
{
        struct lark_t *lark = p;
        
        if (state)
                lark_set_irq(lark, IRQ_16550);
        else
                lark_clear_irq(lark, IRQ_16550);
}

static void lark_uart_send(void *p, uint8_t val)
{
        struct lark_t *lark = p;

        midi_write(lark->midi, val);
}

void lark_midi_receive(void *p, uint8_t val)
{
        struct lark_t *lark = p;
        
        n16550_receive(&lark->n16550, val);
}

void lark_sound_in_start(struct lark_t *lark)
{
        sound_in_start(lark->sound_in);
}

void lark_sound_in_stop(struct lark_t *lark)
{
        sound_in_stop(lark->sound_in);
}

static void lark_sound_in_buffer(void *p, void *buffer, int samples)
{
        struct lark_t *lark = p;
        
        ad1848_in_buffer(&lark->ad1848, buffer, samples);
}

static void lark_set_out_hf(int state, void *p)
{
        lark_t *lark = p;
        
        if (!state)
                lark_set_irq(lark, IRQ_OUT_FIFO);
        else
                lark_clear_irq(lark, IRQ_OUT_FIFO);
}

static void lark_set_in_hf(int state, void *p)
{
        lark_t *lark = p;

//        lark_log("lark_set_in_hf: state=%i\n", state);
        
        if (!state)
                lark_set_irq(lark, IRQ_IN_FIFO);
        else
                lark_clear_irq(lark, IRQ_IN_FIFO);
}

static uint8_t lark_dma_read(lark_t *lark)
{
        if (lark->ctrl & CTRL_OUT_FIFO_ENA)
                return am7202a_read(&lark->out_fifo);
        else
                return 0;
}

static void lark_dma_write(lark_t *lark, uint8_t val)
{
        if (lark->ctrl & CTRL_IN_FIFO_ENA)
                am7202a_write(&lark->in_fifo, val);
}

static int lark_init(struct podule_t *podule)
{
        FILE *f;
        char rom_fn[512];
        lark_t *lark = malloc(sizeof(lark_t));
        memset(lark, 0, sizeof(lark_t));

        sprintf(rom_fn, "%slark.rom", podule_path);
        lark_log("Lark ROM %s\n", rom_fn);
        f = fopen(rom_fn, "rb");
        if (!f)
        {
                lark_log("Failed to open LARK.ROM!\n");
                return -1;
        }
        fread(lark->rom, 0x20000, 1, f);
        fclose(f);
        
        lark->irqstat = 0x80;

        ad1848_init(&lark->ad1848, lark, lark_dma_read, lark_dma_write);
        am7202a_init(&lark->out_fifo, NULL, NULL, lark_set_out_hf, lark);
        am7202a_init(&lark->in_fifo, NULL, NULL, lark_set_in_hf, lark);
        n16550_init(&lark->n16550, MIDI_UART_CLOCK, lark_uart_irq, lark_uart_send, lark, lark_log);

        podule->p = lark;
        lark->podule = podule;

        lark->sound_out = sound_out_init(lark, 48000, 4800, lark_log, podule_callbacks, podule);
        lark->sound_in = sound_in_init(lark, lark_sound_in_buffer, lark_log, podule_callbacks, podule);
        lark->midi = midi_init(lark, lark_midi_receive, lark_log, podule_callbacks, podule);

        return 0;
}

static void lark_close(struct podule_t *podule)
{
        lark_t *lark = podule->p;

        sound_out_close(lark->sound_out);
        sound_in_close(lark->sound_in);
        midi_close(lark->midi);

        free(lark);
}

static podule_config_t lark_config =
{
        .items =
        {
                {
                        .name = "sound_in_device",
                        .description = "Sound input device",
                        .type = CONFIG_SELECTION,
                        .selection = NULL,
                        .default_int = -1
                },
                {
                        .name = "midi_out_device",
                        .description = "MIDI output device",
                        .type = CONFIG_SELECTION,
                        .selection = NULL,
                        .default_int = -1
                },
                {
                        .name = "midi_in_device",
                        .description = "MIDI input device",
                        .type = CONFIG_SELECTION,
                        .selection = NULL,
                        .default_int = -1
                },
                {
                        .type = -1
                }
        }
};

static const podule_header_t lark_podule_header =
{
        .version = PODULE_API_VERSION,
        .flags = PODULE_FLAGS_UNIQUE,
        .short_name = "lark",
        .name = "Computer Concepts Lark",
        .functions =
        {
                .init = lark_init,
                .close = lark_close,
                .read_b = lark_read_b,
                .read_w = lark_read_w,
                .write_b = lark_write_b,
                .write_w = lark_write_w,
                .run = lark_run
        },
        .config = &lark_config
};

const podule_header_t *podule_probe(const podule_callbacks_t *callbacks, char *path)
{
        char dev_name[256];

        podule_callbacks = callbacks;
        strcpy(podule_path, path);

        lark_config.items[0].selection = sound_in_devices_config();
        lark_config.items[1].selection = midi_out_devices_config();
        lark_config.items[2].selection = midi_in_devices_config();

        return &lark_podule_header;
}

#ifdef WIN32
BOOL APIENTRY DllMain (HINSTANCE hInst     /* Library instance handle. */ ,
                       DWORD reason        /* Reason this function is being called. */ ,
                       LPVOID reserved     /* Not used. */ )
{
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
#endif
