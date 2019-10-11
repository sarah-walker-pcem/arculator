#include "timer.h"

typedef struct st506_t
{
        uint8_t status;
        uint8_t param[16];
        uint8_t cul;
        uint8_t command;
        int rp, wp;
        int track[2];
        int lsect, lcyl, lhead, drive;
        int oplen;
        uint8_t OM1;
        uint8_t ssb;
        int drq;
        int first;
        
        int spt[2], hpc[2];

        emu_timer_t timer;
        FILE *hdfile[2];
        uint8_t buffer[272];
        
        void (*irq_raise)(struct st506_t *st506);
        void (*irq_clear)(struct st506_t *st506);
        
        void *p;
} st506_t;

void st506_init(st506_t *st506, char *fn_pri, int pri_spt, int pri_hpc, char *fn_sec, int sec_spt, int sec_hpc, void (*irq_raise)(st506_t *st506), void (*irq_clear)(st506_t *st506), void *p);
void st506_close(st506_t *st506);
uint8_t st506_readb(st506_t *st506, uint32_t addr);
uint32_t st506_readl(st506_t *st506, uint32_t addr);
void st506_writeb(st506_t *st506, uint32_t addr, uint8_t val);
void st506_writel(st506_t *st506, uint32_t addr, uint32_t val);


void st506_internal_init(void);
void st506_internal_close(void);
uint8_t st506_internal_readb(uint32_t addr);
uint32_t st506_internal_readl(uint32_t addr);
void st506_internal_writeb(uint32_t addr, uint8_t val);
void st506_internal_writel(uint32_t addr, uint32_t val);

extern int st506_present;
