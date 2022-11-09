#include <stdio.h>
#include <stdint.h>
#include "6850.h"

#define STATUS_RDRF (1 << 0) /*Receive data register full*/
#define STATUS_TDRE (1 << 1) /*Transmit data register empty*/
#define STATUS_IRQ  (1 << 7)

#define CTRL_TCTRL_MASK (3 << 5) /*Transmit control*/
#define CTRL_RIE (1 << 7) /*Receive interrupt enable*/

#define CTRL_TCTRL_IRQ_ENA (1 << 5) /*Transmit interrupt enable*/

void m6850_init(m6850_t *m6850, int input_clock, void (*set_irq)(void *p, int state), void (*tx_data)(void *p, uint8_t val), void *p, void (*log)(const char *format, ...))
{
	m6850->set_irq = set_irq;
	m6850->tx_data = tx_data;
	m6850->p = p;
	m6850->log = log;
	m6850->input_clock = input_clock;
	m6850->baud_rate = m6850->input_clock / (3 * 16);

	m6850->status = STATUS_TDRE;
}

static void update_ints(m6850_t *m6850)
{
	int irq = 0;

	if ((m6850->status & STATUS_RDRF) && (m6850->ctrl & CTRL_RIE))
		irq = 1;
	if ((m6850->status & STATUS_TDRE) && ((m6850->ctrl & CTRL_TCTRL_MASK) == CTRL_TCTRL_IRQ_ENA))
		irq = 1;

	if (m6850->log)
		m6850->log("update_ints: status=%02x ctrl=%02x %02x irq=%i\n", m6850->status, m6850->ctrl, m6850->ctrl & CTRL_TCTRL_MASK, irq);

	m6850->set_irq(m6850->p, irq);

	if (irq)
		m6850->status |= STATUS_IRQ;
	else
		m6850->status &= ~STATUS_IRQ;
}

uint8_t m6850_read(m6850_t *m6850, uint32_t addr)
{
	uint8_t temp = 0xff;

	if (!(addr & 1))
	{
		/*Status*/
		temp = m6850->status;
	}
	else
	{
		/*Receive data*/
		temp = m6850->rx_data;
		m6850->status &= ~STATUS_RDRF;
		update_ints(m6850);
	}

	return temp;
}

void m6850_write(m6850_t *m6850, uint32_t addr, uint8_t val)
{
	if (!(addr & 1))
	{
		/*Control*/
		m6850->ctrl = val;
		update_ints(m6850);
	}
	else
	{
		/*Transmit data*/
		m6850->tx_data(m6850->p, val);
		m6850->tx_irq_pending = (1000000 * 10) / m6850->baud_rate;

		m6850->status &= ~STATUS_TDRE;
		update_ints(m6850);
	}
}

void m6850_run(m6850_t *m6850, int timeslice_us)
{
	if (m6850->tx_irq_pending)
	{
		m6850->tx_irq_pending -= timeslice_us;

		if (m6850->tx_irq_pending <= 0)
		{
//		        if (m6850->log)
//        			m6850->log("  tx irq\n");

			m6850->tx_irq_pending = 0;
			m6850->status |= STATUS_TDRE;
			update_ints(m6850);
		}
	}
	if (m6850->rx_pending)
	{
		m6850->rx_pending -= timeslice_us;
		if (m6850->rx_pending <= 0)
			m6850->rx_pending = 0;
	}
	if (!m6850->rx_pending && m6850->rx_rp != m6850->rx_wp)
	{
		m6850->rx_data = m6850->rx_queue[m6850->rx_rp & 0xff];
		m6850->rx_rp++;

		m6850->status |= STATUS_RDRF;
		update_ints(m6850);

		m6850->rx_pending = (1000000 * 10) / m6850->baud_rate;
	}
}

void m6850_receive(m6850_t *m6850, uint8_t val)
{
	if ((m6850->rx_wp - m6850->rx_rp) >= 256)
	{
//                lark_log("m6850 drop %02x\n", val);
		return;
	}

//        lark_log("m6850 receive %02x %i %i\n", val, m6850->rx_wp, m6850->rx_rp);
	m6850->rx_queue[m6850->rx_wp & 0xff] = val;
	m6850->rx_wp++;
}
