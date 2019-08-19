#ifndef _DLL_H_
#define _DLL_H_

#if BUILDING_DLL
# define DLLIMPORT __declspec (dllexport)
#else /* Not BUILDING_DLL */
# define DLLIMPORT __declspec (dllimport)
#endif /* Not BUILDING_DLL */

void lark_log(const char *format, ...);

#define IRQ_MASTER (1 << 0)
#define IRQ_AD1848 (1 << 1)
#define IRQ_16550  (1 << 2)
#define IRQ_OUT_FIFO (1 << 6)
#define IRQ_IN_FIFO  (1 << 7)

struct lark_t;
void lark_set_irq(struct lark_t *lark, uint8_t irq);
void lark_clear_irq(struct lark_t *lark, uint8_t irq);

void lark_midi_send(struct lark_t *lark, uint8_t val);

void lark_sound_in_start(struct lark_t *lark);
void lark_sound_in_stop(struct lark_t *lark);

void lark_sound_out_buffer(struct lark_t *lark, void *buffer, int samples);

int wss_irq();

#endif /* _DLL_H_ */
