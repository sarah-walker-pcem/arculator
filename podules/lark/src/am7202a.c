#include <stdint.h>
#include "am7202a.h"

void am7202a_reset(am7202a_t *fifo)
{
        fifo->wp = 0;
        fifo->rp = 0;
}

void am7202a_init(am7202a_t *fifo, void (*set_ef)(int state, void *p), void (*set_ff)(int state, void *p), void (*set_hf)(int state, void *p), void *p)
{
        fifo->p = p;
        fifo->set_ef = set_ef;
        fifo->set_ff = set_ff;
        fifo->set_hf = set_hf;

        am7202a_reset(fifo);
}

void am7202a_write(am7202a_t *fifo, uint16_t val)
{
        if ((fifo->wp - fifo->rp) == 1024) /*FIFO full?*/
                return;

        if (fifo->set_ef && fifo->wp == fifo->rp)
                fifo->set_ef(0, fifo->p); /*Clear empty flag*/

//        lark_log("am7202a_write: val=%02x rp=%03x wp=%03x\n", val, fifo->rp, fifo->wp);
        fifo->data[fifo->wp & 0x3ff] = val;
        fifo->wp++;
        
        if (fifo->set_ff && (fifo->wp - fifo->rp) == 1024)
                fifo->set_ff(1, fifo->p); /*Set full flag*/

        if (fifo->set_hf && (fifo->wp - fifo->rp) == 512)
                fifo->set_hf(1, fifo->p); /*Set half full flag*/
}

uint16_t am7202a_read(am7202a_t *fifo)
{
        uint16_t val;
        
        if (fifo->wp == fifo->rp)
                return 0; /*FIFO empty*/

        if (fifo->set_ff && (fifo->wp - fifo->rp) == 1024)
                fifo->set_ff(0, fifo->p); /*Clear full flag*/

        if (fifo->set_hf && (fifo->wp - fifo->rp) == 512)
                fifo->set_hf(0, fifo->p); /*Clear half full flag*/

        val = fifo->data[fifo->rp & 0x3ff];
//        lark_log("am7202a_read: val=%02x rp=%03x wp=%03x\n", val, fifo->rp, fifo->wp);
        fifo->rp++;

        if (fifo->set_ef && fifo->wp == fifo->rp)
                fifo->set_ef(1, fifo->p); /*Set empty flag*/

        return val;
}
