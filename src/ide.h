#include "timer.h"

typedef struct ide_t
{
        uint8_t atastat;
        uint8_t error,status;
        int secount,sector,cylinder,head,drive,cylprecomp;
        uint8_t command;
        uint8_t fdisk;
        int pos;
        int spt[2],hpc[2];
        int reset;
        FILE *hdfile[2];
        uint16_t idebuffer[256];
        uint8_t *idebufferb;
        int skip512[2];
        timer_t timer;

        void (*irq_raise)(struct ide_t *ide);
        void (*irq_clear)(struct ide_t *ide);
} ide_t;

extern ide_t ide_internal;

uint16_t readidew(ide_t *ide);
void writeidew(ide_t *ide, uint16_t val);
uint8_t readide(ide_t *ide, uint32_t addr);
void writeide(ide_t *ide, uint32_t addr, uint8_t val);
void callbackide(void *p);

void resetide(ide_t *ide, char *fn_pri, char *fn_sec, void (*irq_raise)(ide_t *ide), void (*irq_clear)(ide_t *ide));
void resetide_drive(ide_t *ide);
void closeide(ide_t *ide);
