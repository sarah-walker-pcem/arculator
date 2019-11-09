#ifdef WIN32
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include "midimax.h"
#include "16550.h"
#include "midi.h"
#include "podule_api.h"

#ifdef WIN32
extern __declspec(dllexport) const podule_header_t *podule_probe(const podule_callbacks_t *callbacks, char *path);
#else
#define BOOL int
#define APIENTRY
#endif

static const podule_callbacks_t *podule_callbacks;
char podule_path[512];

#define MIDI_UART_CLOCK 2000000 //(31250Hz * 4 * 16)

#ifdef DEBUG_LOG
static FILE *midimax_logf;
#endif

void midimax_log(const char *format, ...)
{
#ifdef DEBUG_LOG
   char buf[1024];
//return;
   if (!midimax_logf) midimax_logf=fopen("midimax_log.txt","wt");
   va_list ap;
   va_start(ap, format);
   vsprintf(buf, format, ap);
   va_end(ap);
   fputs(buf,midimax_logf);
   fflush(midimax_logf);
#endif
}

typedef struct midimax_t
{
        uint8_t rom[0x8000];
        int rom_page;
        
        int tx_irq_pending;
        uint8_t intena, intstat;
        
        n16550_t n16550;
        
        podule_t *podule;
        void *midi;
} midimax_t;

static uint8_t midimax_read_b(struct podule_t *podule, podule_io_type type, uint32_t addr)
{
        midimax_t *midimax = podule->p;
        uint8_t temp = 0xff;

        if (type != PODULE_IO_TYPE_IOC)
                return 0xff;

        switch (addr&0x3000)
        {
                case 0x0000: case 0x1000:
                return midimax->rom[((midimax->rom_page * 2048) + ((addr & 0x1fff) >> 2)) & 0x7fff];

                case 0x3000:
                return n16550_read(&midimax->n16550, (addr >> 2) & 7);
        }
        return 0xFF;
}

static void midimax_write_b(struct podule_t *podule, podule_io_type type, uint32_t addr, uint8_t val)
{
        midimax_t *midimax = podule->p;

        if (type != PODULE_IO_TYPE_IOC)
                return;

        switch (addr & 0x3000)
        {
                case 0x2000:
                midimax->rom_page = val;
                return;
                case 0x3000: /*16550*/
                n16550_write(&midimax->n16550, (addr >> 2) & 7, val);
                break;
        }
}

static int midimax_run(struct podule_t *podule, int timeslice_us)
{
        midimax_t *midimax = podule->p;

        n16550_run(&midimax->n16550, timeslice_us);

        return 256; /*256us = 1 byte at 31250 baud*/
}

static void midimax_uart_irq(void *p, int state)
{
        midimax_t *midimax = p;
        podule_t *podule = midimax->podule;
        
        podule_callbacks->set_irq(podule, state);
}

static void midimax_uart_send(void *p, uint8_t val)
{
        struct midimax_t *midimax = p;

        midi_write(midimax->midi, val);
}

static void midimax_midi_receive(void *p, uint8_t val)
{
        struct midimax_t *midimax = p;

        n16550_receive(&midimax->n16550, val);
}

static int midimax_init(struct podule_t *podule)
{
        FILE *f;
        char rom_fn[512];
        midimax_t *midimax = malloc(sizeof(midimax_t));
        memset(midimax, 0, sizeof(midimax_t));
        
        sprintf(rom_fn, "%smidimax.rom", podule_path);
        midimax_log("MIDIMAX ROM %s\n", rom_fn);
        f = fopen(rom_fn, "rb");
        if (!f)
        {
                midimax_log("Failed to open MIDIMAX.ROM!\n");
                return -1;
        }
        fread(midimax->rom, 0x8000, 1, f);
        fclose(f);
        
        n16550_init(&midimax->n16550, MIDI_UART_CLOCK, midimax_uart_irq, midimax_uart_send, midimax, midimax_log);
        
        midimax->midi = midi_init(midimax, midimax_midi_receive, midimax_log, podule_callbacks, podule);
        
        midimax->podule = podule;
        podule->p = midimax;
        
        return 0;
}

static void midimax_close(struct podule_t *podule)
{
        midimax_t *midimax = podule->p;
        
        midi_close(midimax->midi);

        free(midimax);
}

static podule_config_t midimax_config =
{
        .items =
        {
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

static const podule_header_t midimax_podule_header =
{
        .version = PODULE_API_VERSION,
        .flags = PODULE_FLAGS_UNIQUE,
        .short_name = "midimax",
        .name = "Wild Vision MIDI Max",
        .functions =
        {
                .init = midimax_init,
                .close = midimax_close,
                .read_b = midimax_read_b,
                .write_b = midimax_write_b,
                .run = midimax_run
        },
        .config = &midimax_config
};

const podule_header_t *podule_probe(const podule_callbacks_t *callbacks, char *path)
{
        podule_callbacks = callbacks;
        strcpy(podule_path, path);
        
        midimax_config.items[0].selection = midi_out_devices_config();
        midimax_config.items[1].selection = midi_in_devices_config();

        return &midimax_podule_header;
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
