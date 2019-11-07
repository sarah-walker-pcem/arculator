/*Arculator 2.0 by Sarah Walker
  Podule subsystem*/
#include <stdlib.h>
#include <string.h>

#include "arc.h"
#include "arcrom.h"
#include "config.h"
#include "ide_idea.h"
#include "ide_riscdev.h"
#include "ide_zidefs.h"
#include "ioc.h"
#include "podules.h"
#include "st506_akd52.h"
#include "timer.h"

static uint8_t backplane_mask;
static void podule_run_timer(void *p);

typedef struct podule_list
{
        const podule_header_t *header;
        struct podule_list *next;
} podule_list;

static podule_list *podule_list_head = NULL;

static const podule_header_t *(*internal_podules[])(const podule_callbacks_t *callbacks, char *path) =
{
        akd52_probe,
        arcrom_probe,
        idea_ide_probe,
        riscdev_ide_probe,
        zidefs_ide_probe
};

#define NR_INTERNAL_PODULES (sizeof(internal_podules) / sizeof(internal_podules[0]))
static int nr_podules = 0;

/*Podules -
  0 is reserved for extension ROMs
  1 is for additional IDE interface
  2-3 are free*/
static podule_internal_state_t podules[4];
static const podule_functions_t *podule_functions[4];
char podule_names[4][16];

void podule_add(const podule_header_t *header)
{
        podule_list *current = malloc(sizeof(podule_list));
        podule_list *last_entry = podule_list_head;
        podule_list *prev_entry = NULL;
        
        current->header = header;
        current->next = NULL;

        if (!last_entry)
                podule_list_head = current;
        else while (last_entry)
        {
                if (strcasecmp(header->name, last_entry->header->name) < 0)
                {
                        current->next = last_entry;
                        if (prev_entry)
                        {
                                /*Insert before last_entry*/
                                prev_entry->next = current;
                        }
                        else
                        {
                                /*Insert as head of list*/
                                podule_list_head = current;
                        }
                        break;
                }

                if (!last_entry->next)
                {
                        /*Insert at end of list*/
                        last_entry->next = current;
                        break;
                }
                
                prev_entry = last_entry;
                last_entry = last_entry->next;
        }

        nr_podules++;
}

void podule_build_list(void)
{
        int c;

        for (c = 0; c < NR_INTERNAL_PODULES; c++)
        {
                const podule_header_t *header = internal_podules[c](&podule_callbacks_def, exname);
                
                if (header)
                        podule_add(header);
        }
}

const char *podule_get_name(int c)
{
        podule_list *current = podule_list_head;

        while (c--)
        {
                current = current->next;
                if (!current)
                        return NULL;
        }
        
        return current->header->name;
}

const char *podule_get_short_name(int c)
{
        podule_list *current = podule_list_head;

        while (c--)
        {
                current = current->next;
                if (!current)
                        return NULL;
        }

        return current->header->short_name;
}

uint32_t podule_get_flags(int c)
{
        podule_list *current = podule_list_head;

        while (c--)
        {
                current = current->next;
                if (!current)
                        return 0;
        }

        return current->header->flags;
}

const podule_header_t *podule_find(const char *short_name)
{
        podule_list *current = podule_list_head;

        while (current)
        {
                if (!strcmp(short_name, current->header->short_name))
                        return current->header;
                current = current->next;
        }
        
        return NULL;
}

void podules_init(void)
{
        int c;
        
        podules_close();
        
        for (c = 0; c < 4; c++)
        {
                const podule_header_t *header = podule_find(podule_names[c]);
                
                memset(&podules[c], 0, sizeof(podule_internal_state_t));
                
                if (header)
                {
                        podules[c].podule.header = header;
                        podule_functions[c] = &header->functions;
                        
                        if (podule_functions[c]->init)
                        {
                                int ret = podule_functions[c]->init(&podules[c].podule);
                                
                                if (ret)
                                {
                                        /*Podule init failed, clear structs*/
                                        rpclog("Failed to init podule %i : %s\n", c, header->short_name);
                                        podules[c].podule.header = NULL;
                                        podule_functions[c] = NULL;
                                }
                                else
                                {
                                        timer_add(&podules[c].timer, podule_run_timer, (void *)c, 1);
                                        podules[c].last_callback_tsc = tsc;
                                }
                        }
                }
        }
}

void podules_reset(void)
{
        int c;

	for (c = 0; c < 4; c++)
        {
                if (podule_functions[c] && podule_functions[c]->reset)
                        podule_functions[c]->reset(&podules[c].podule);
        }
        backplane_mask = 0xf; /*All IRQs enabled*/
}

/**
 * Reset and empty all the podule slots
 *
 * Safe to call on program startup and user instigated virtual machine
 * reset.
 */
void podules_close(void)
{
	int c;

	/* Call any reset functions that an open podule may have to allow
	   then to tidy open files etc */
	for (c = 0; c < 4; c++)
        {
                if (podule_functions[c] && podule_functions[c]->close)
                        podule_functions[c]->close(&podules[c].podule);

                podules[c].podule.header = NULL;
                podule_functions[c] = NULL;
	}
}
  
void rethinkpoduleints(void)
{
        int c;
        ioc.irqb &= ~(0x21);
        ioc.fiq  &= ~0x40;
        for (c=0;c<4;c++)
        {
                if (podules[c].irq && (backplane_mask & (1 << c)))
                {
//                        rpclog("Podule IRQ! %02X %i\n", ioc.mskb, c);
                        ioc.irqb |= 0x20;
                }
                if (podules[c].fiq)
                {
                        ioc.irqb |= 0x01;
                        ioc.fiq  |= 0x40;
                }
        }
        ioc_updateirqs();
}

void podule_set_irq(podule_t *podule, int state)
{
        podule_internal_state_t *internal = container_of(podule, podule_internal_state_t, podule);

        internal->irq = state;
        rethinkpoduleints();
}
void podule_set_fiq(podule_t *podule, int state)
{
        podule_internal_state_t *internal = container_of(podule, podule_internal_state_t, podule);

        internal->fiq = state;
        rethinkpoduleints();
}

void podule_write_b(int num, uint32_t addr, uint8_t val)
{
        if (podule_functions[num] && podule_functions[num]->write_b)
                podule_functions[num]->write_b(&podules[num].podule, PODULE_IO_TYPE_IOC, addr, val);
}

void podule_write_w(int num, uint32_t addr, uint32_t val)
{
//        rpclog("podule_write_w: addr=%08x val=%08x\n", addr, val);
        if (podule_functions[num] && podule_functions[num]->write_w)
                podule_functions[num]->write_w(&podules[num].podule, PODULE_IO_TYPE_IOC, addr, val >> 16);
}

void podule_memc_write_b(int num, uint32_t addr, uint8_t val)
{
        if (podule_functions[num] && podule_functions[num]->write_b)
                podule_functions[num]->write_b(&podules[num].podule, PODULE_IO_TYPE_MEMC, addr, val);
}

void podule_memc_write_w(int num, uint32_t addr, uint32_t val)
{
//        rpclog("podule_memc_write_w: addr=%08x val=%08x\n", addr, val);
        if (podule_functions[num] && podule_functions[num]->write_w)
                podule_functions[num]->write_w(&podules[num].podule, PODULE_IO_TYPE_MEMC, addr, val >> 16);
}


uint8_t podule_read_b(int num, uint32_t addr)
{
        uint8_t temp = 0xff;
        
        if (podule_functions[num] && podule_functions[num]->read_b)
                temp = podule_functions[num]->read_b(&podules[num].podule, PODULE_IO_TYPE_IOC, addr);

        return temp;
}

uint32_t podule_read_w(int num, uint32_t addr)
{
        uint16_t temp = 0xffff;

        if (podule_functions[num] && podule_functions[num]->read_w)
                temp = podule_functions[num]->read_w(&podules[num].podule, PODULE_IO_TYPE_IOC, addr);

        return temp;
}

uint8_t podule_memc_read_b(int num, uint32_t addr)
{
        uint8_t temp = 0xff;

        if (podule_functions[num] && podule_functions[num]->read_b)
                temp = podule_functions[num]->read_b(&podules[num].podule, PODULE_IO_TYPE_MEMC, addr);

        return temp;
}

uint32_t podule_memc_read_w(int num, uint32_t addr)
{
        uint16_t temp = 0xffff;

        if (podule_functions[num] && podule_functions[num]->read_w)
                temp = podule_functions[num]->read_w(&podules[num].podule, PODULE_IO_TYPE_MEMC, addr);

        return temp;
}

static void podule_run_timer(void *p)
{
        int num = (int)p;
        podule_t *podule = &podules[num].podule;
        uint64_t timeslice = tsc - podules[num].last_callback_tsc;
        int ret = 0;

        podules[num].last_callback_tsc = tsc;
        if (podule_functions[num]->run)
                ret = podule_functions[num]->run(podule, timeslice / TIMER_USEC);

        if (ret)
                timer_advance_u64(&podules[num].timer, ret * TIMER_USEC);
}

uint8_t podule_irq_state()
{
        uint8_t state = 0;

        if (podules[0].irq)
                state |= 0x01;
        if (podules[1].irq)
                state |= 0x02;
        if (podules[2].irq)
                state |= 0x04;
        if (podules[3].irq)
                state |= 0x08;
        state &= backplane_mask;

        return state;
}

void podule_write_backplane_mask(uint8_t val)
{
        backplane_mask = val;
        rethinkpoduleints();
}

uint8_t podule_read_backplane_mask(void)
{
        return backplane_mask;
}

static int podule_get_nr(podule_t *podule)
{
        podule_internal_state_t *internal = container_of(podule, podule_internal_state_t, podule);
        
        return ((uintptr_t)internal - (uintptr_t)&podules[0]) / sizeof(podule_internal_state_t);
}

static int podule_config_get_int(podule_t *podule, const char *name, int def)
{
        char section_name[20];
        int slot_nr = podule_get_nr(podule);
        
        snprintf(section_name, 20, "%s.%i", podule->header->short_name, slot_nr);
        
        return config_get_int(CFG_MACHINE, section_name, name, def);
}

static const char *podule_config_get_string(podule_t *podule, const char *name, const char *def)
{
        char section_name[20];
        int slot_nr = podule_get_nr(podule);

        snprintf(section_name, 20, "%s.%i", podule->header->short_name, slot_nr);

        return config_get_string(CFG_MACHINE, section_name, name, def);
}

static void podule_config_set_int(podule_t *podule, const char *name, int val)
{
        char section_name[20];
        int slot_nr = podule_get_nr(podule);

        snprintf(section_name, 20, "%s.%i", podule->header->short_name, slot_nr);

        config_set_int(CFG_MACHINE, section_name, name, val);
}

static void podule_config_set_string(podule_t *podule, const char *name, char *val)
{
        char section_name[20];
        int slot_nr = podule_get_nr(podule);

        snprintf(section_name, 20, "%s.%i", podule->header->short_name, slot_nr);

        config_set_string(CFG_MACHINE, section_name, name, val);
}

static void podule_set_timer_delay_us(podule_t *podule, int delay_us)
{
        podule_internal_state_t *internal = container_of(podule, podule_internal_state_t, podule);

        timer_set_delay_u64(&internal->timer, delay_us * TIMER_USEC);
}

static int podule_get_timer_remaining_us(podule_t *podule)
{
        podule_internal_state_t *internal = container_of(podule, podule_internal_state_t, podule);

        return timer_get_remaining_us(&internal->timer);
}

static void podule_stop_timer(podule_t *podule)
{
        podule_internal_state_t *internal = container_of(podule, podule_internal_state_t, podule);
        
        timer_disable(&internal->timer);
}

const podule_callbacks_t podule_callbacks_def =
{
        .set_irq = podule_set_irq,
        .set_fiq = podule_set_fiq,
        .set_timer_delay_us = podule_set_timer_delay_us,
        .get_timer_remaining_us = podule_get_timer_remaining_us,
        .stop_timer = podule_stop_timer,
        .config_get_int = podule_config_get_int,
        .config_get_string = podule_config_get_string,
        .config_set_int = podule_config_set_int,
        .config_set_string = podule_config_set_string,
        .config_get_current = podule_config_get_current,
        .config_set_current = podule_config_set_current,
        .config_file_selector = podule_config_file_selector,
        .config_open = podule_config_open
};
