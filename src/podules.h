#ifndef PODULES_H
#define PODULES_H

void writepodulel(int num, int easi, uint32_t addr, uint32_t val);
void writepodulew(int num, int easi, uint32_t addr, uint32_t val);
void writepoduleb(int num, int easi, uint32_t addr, uint8_t val);
uint32_t  readpodulel(int num, int easi, uint32_t addr);
uint32_t readpodulew(int num, int easi, uint32_t addr);
uint8_t  readpoduleb(int num, int easi, uint32_t addr);

void podule_memc_writew(int num, uint32_t addr, uint32_t val);
void podule_memc_writeb(int num, uint32_t addr, uint8_t  val);
uint32_t podule_memc_readw(int num, uint32_t addr);
uint8_t  podule_memc_readb(int num, uint32_t addr);

typedef struct podule
{
        void (*writeb)(struct podule *p, int easi, uint32_t addr, uint8_t val);
        void (*writew)(struct podule *p, int easi, uint32_t addr, uint16_t val);
        void (*writel)(struct podule *p, int easi, uint32_t addr, uint32_t val);
        uint8_t  (*readb)(struct podule *p, int easi, uint32_t addr);
        uint16_t (*readw)(struct podule *p, int easi, uint32_t addr);
        uint32_t (*readl)(struct podule *p, int easi, uint32_t addr);
        void (*memc_writeb)(struct podule *p, uint32_t addr, uint8_t val);
        void (*memc_writew)(struct podule *p, uint32_t addr, uint16_t val);
        uint8_t  (*memc_readb)(struct podule *p, uint32_t addr);
        uint16_t (*memc_readw)(struct podule *p, uint32_t addr);
        int (*timercallback)(struct podule *p);
        void (*reset)(struct podule *p);
        int irq,fiq;
        int msectimer;
        int broken;
} podule;

void rethinkpoduleints(void);

podule *addpodule(void (*writel)(podule *p, int easi, uint32_t addr, uint32_t val),
              void (*writew)(podule *p, int easi, uint32_t addr, uint16_t val),
              void (*writeb)(podule *p, int easi, uint32_t addr, uint8_t val),
              uint32_t (*readl)(podule *p, int easi, uint32_t addr),
              uint16_t (*readw)(podule *p, int easi, uint32_t addr),
              uint8_t  (*readb)(podule *p, int easi, uint32_t addr),
              void (*memc_writew)(podule *p, uint32_t addr, uint16_t val),
              void (*memc_writeb)(podule *p, uint32_t addr, uint8_t val),
              uint16_t (*memc_readw)(podule *p, uint32_t addr),
              uint8_t  (*memc_readb)(podule *p, uint32_t addr),
              int (*timercallback)(podule *p),
              void (*reset)(podule *p),
              int broken);

void runpoduletimers(int t);
void podules_reset(void);
uint8_t podule_irq_state();

#endif
