#ifndef PODULES_H
#define PODULES_H

#include "podule_api.h"
#include "timer.h"

void podules_init(void);
void podules_reset(void);
void podules_close(void);
void podule_add(const podule_header_t *header);

void podule_write_b(int num, uint32_t addr, uint8_t val);
void podule_write_w(int num, uint32_t addr, uint32_t val);
uint8_t  podule_read_b(int num, uint32_t addr);
uint32_t podule_read_w(int num, uint32_t addr);

void podule_memc_write_w(int num, uint32_t addr, uint32_t val);
void podule_memc_write_b(int num, uint32_t addr, uint8_t  val);
uint32_t podule_memc_read_w(int num, uint32_t addr);
uint8_t  podule_memc_read_b(int num, uint32_t addr);

void podule_write_backplane_mask(uint8_t val);
uint8_t podule_read_backplane_mask(void);

void podule_build_list(void);
const char *podule_get_name(int c);
const char *podule_get_short_name(int c);
uint32_t podule_get_flags(int c);
const podule_header_t *podule_find(const char *short_name);

extern char podule_names[4][16];

typedef struct podule_internal_state_t
{
        podule_t podule;
        int irq, fiq;
        timer_t timer;
        uint64_t last_callback_tsc;
} podule_internal_state_t;

void rethinkpoduleints(void);

void podules_reset(void);
uint8_t podule_irq_state();

void opendlls(void);

void podule_set_irq(podule_t *podule, int state);

extern const podule_callbacks_t podule_callbacks_def;

#endif
