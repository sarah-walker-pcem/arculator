#include <stdio.h>
#include <stdint.h>
#include "scc2691.h"

#define ISR_TxRDY (1 << 0) /*Transmit data empty*/
#define ISR_RxRDY (1 << 2) /*Receive data full*/

void scc2691_init(scc2691_t *scc2691, int input_clock, void (*set_irq)(void *p, int state), void (*tx_data)(void *p, uint8_t val), void *p, void (*log)(const char *format, ...))
{
	scc2691->set_irq = set_irq;
	scc2691->tx_data = tx_data;
	scc2691->p = p;
	scc2691->log = log;
	scc2691->input_clock = input_clock;
	scc2691->baud_rate = scc2691->input_clock / (3 * 16);

	scc2691->isr = ISR_TxRDY;
}

static void update_ints(scc2691_t *scc2691)
{
//        if (scc2691->log)
//        	scc2691->log("update_ints: %02x %02x\n", scc2691->isr, scc2691->imr);
	scc2691->set_irq(scc2691->p, scc2691->isr & scc2691->imr);
}

uint8_t scc2691_read(scc2691_t *scc2691, uint32_t addr)
{
	uint8_t temp = 0xff;

	switch (addr & 7)
	{
		case 3: /*Receive data*/
		temp = scc2691->rx_data;
		scc2691->isr &= ~ISR_RxRDY;
		update_ints(scc2691);
		break;

		case 5:
		temp = scc2691->isr;
		break;
	}

	return temp;
}

void scc2691_write(scc2691_t *scc2691, uint32_t addr, uint8_t val)
{
//        if (scc2691->log)
//                scc2691->log("scc2691_write: addr=%01x val=%02x\n", addr & 7, val);
	switch (addr & 7)
	{
		case 3: /*Transmit data*/
		scc2691->tx_data(scc2691->p, val);
		scc2691->tx_irq_pending = (1000000 * 10) / scc2691->baud_rate;

		scc2691->isr &= ~ISR_TxRDY;
		update_ints(scc2691);
		break;

		case 5:
		scc2691->imr = val;
		update_ints(scc2691);
		break;
	}
}

void scc2691_run(scc2691_t *scc2691, int timeslice_us)
{
	if (scc2691->tx_irq_pending)
	{
		scc2691->tx_irq_pending -= timeslice_us;

		if (scc2691->tx_irq_pending <= 0)
		{
//		        if (scc2691->log)
//        			scc2691->log("  tx irq\n");

			scc2691->tx_irq_pending = 0;
			scc2691->isr |= ISR_TxRDY;
			update_ints(scc2691);
		}
	}
	if (scc2691->rx_pending)
	{
		scc2691->rx_pending -= timeslice_us;
		if (scc2691->rx_pending <= 0)
			scc2691->rx_pending = 0;
	}
	if (!scc2691->rx_pending && scc2691->rx_rp != scc2691->rx_wp)
	{
		scc2691->rx_data = scc2691->rx_queue[scc2691->rx_rp & 0xff];
		scc2691->rx_rp++;

		scc2691->isr |= ISR_RxRDY;
		update_ints(scc2691);

		scc2691->rx_pending = (1000000 * 10) / scc2691->baud_rate;
	}
}

void scc2691_receive(scc2691_t *scc2691, uint8_t val)
{
	if ((scc2691->rx_wp - scc2691->rx_rp) >= 256)
	{
//                lark_log("scc2691 drop %02x\n", val);
		return;
	}

//        lark_log("scc2691 receive %02x %i %i\n", val, scc2691->rx_wp, scc2691->rx_rp);
	scc2691->rx_queue[scc2691->rx_wp & 0xff] = val;
	scc2691->rx_wp++;
}
