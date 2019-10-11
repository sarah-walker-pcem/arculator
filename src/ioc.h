#include "timer.h"

typedef struct IOC_t
{
        uint8_t irqa,irqb,fiq;
        uint8_t mska,mskb,mskf;
        uint8_t ctrl;
        int timerc[4],timerl[4],timerr[4];
        emu_timer_t timers[2];
} IOC_t;
extern IOC_t ioc;

extern void ioc_write(uint32_t addr, uint32_t v);
extern uint8_t ioc_read(uint32_t addr);

extern void ioc_reset();
extern void ioc_updateirqs();

extern void ioc_irqa(uint8_t v);
extern void ioc_irqac(uint8_t v);

extern void ioc_irqb(uint8_t v);
extern void ioc_irqbc(uint8_t v);

extern void ioc_fiq(uint8_t num);
extern void ioc_fiqc(uint8_t num);

extern void ioc_discchange(int drive);
extern void ioc_discchange_clear(int drive);

extern void initjoy();
extern void polljoy();
extern uint8_t readjoy(int addr);

enum
{
        IOC_IRQA_PRINTER_BUSY   = 1 << 0,       /*Old*/
        IOC_IRQA_PRINTER_LATCH  = 1 << 0,       /*New*/
        IOC_IRQA_SERIAL_RING    = 1 << 1,       /*Old*/
        IOC_IRQA_PRINTER_ACK    = 1 << 2,       /*Old*/
        IOC_IRQA_DISC_INDEX     = 1 << 2,       /*New*/
        IOC_IRQA_VBLANK         = 1 << 3,
        IOC_IRQA_POWER_ON       = 1 << 4,
        IOC_IRQA_TIMER_0        = 1 << 5,
        IOC_IRQA_TIMER_1        = 1 << 6,
        IOC_IRQA_FORCE_IRQ      = 1 << 7
};

enum
{
        IOC_IRQB_PODULE_FIQ     = 1 << 0,
        IOC_IRQB_SOUND_BUFFER   = 1 << 1,
        IOC_IRQB_SERIAL_CONTROL = 1 << 2,
        IOC_IRQB_ST506          = 1 << 3,       /*Old*/
        IOC_IRQB_IDE            = 1 << 3,       /*New*/
        IOC_IRQB_DISC_CHANGED   = 1 << 4,       /*Old*/
        IOC_IRQB_DISC_IRQ       = 1 << 4,       /*New*/
        IOC_IRQB_PODULE_IRQ     = 1 << 5,
        IOC_IRQB_KEYBOARD_TX    = 1 << 6,
        IOC_IRQB_KEYBOARD_RX    = 1 << 7
};

enum
{
        IOC_FIQ_DISC_DATA       = 1 << 0,
        IOC_FIQ_DISC_IRQ        = 1 << 1,       /*Old*/
        IOC_FIQ_ECONET          = 1 << 2,
        IOC_FIQ_SERIAL_LINE     = 1 << 4,       /*New*/
        IOC_FIQ_PODULE_FIQ      = 1 << 6,
        IOC_FIQ_FORCE_FIQ       = 1 << 7
};


void initjoy();
void polljoy();
uint8_t readjoy(int addr);

extern int ref8m_period;
