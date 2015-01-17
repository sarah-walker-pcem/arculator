#ifndef PODULES_WIN_H
#define PODULES_WIN_H

#ifdef __cplusplus
extern "C" {
#endif

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
              

extern void opendlls(void);

#ifdef __cplusplus
}
#endif

#endif /* ! PODULES_WIN_H */
