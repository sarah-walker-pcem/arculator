#include <stdio.h>
#include <stdint.h>
#include "16550.h"

enum
{
        SERIAL_INT_LSR = 1,
        SERIAL_INT_RECEIVE = 2,
        SERIAL_INT_TRANSMIT = 4,
        SERIAL_INT_MSR = 8
};

#define IER_RX  (1 << 0) /*Receive data full*/
#define IER_TX  (1 << 1) /*Transmit data empty*/
#define IER_LSR (1 << 2) /*Line status*/
#define IER_MSR (1 << 3) /*Modem status*/

#define LSR_RX_READY (1 << 0) /*Receive register full*/
#define LSR_TX_EMPTY (1 << 5) /*Transmitter holding register empty*/

#define IIR_NO_IRQ 1
#define IIR_LSR    6
#define IIR_RX     4
#define IIR_TX     2
#define IIR_MSR    0

void n16550_init(n16550_t *n16550, int input_clock, void (*set_irq)(void *p, int state), void (*tx_data)(void *p, uint8_t val), void *p, void (*log)(const char *format, ...))
{
        n16550->set_irq = set_irq;
        n16550->tx_data = tx_data;
        n16550->p = p;
        n16550->log = log;
        n16550->input_clock = input_clock;
        n16550->baud_rate = n16550->input_clock / (3 * 16);
}

static void update_ints(n16550_t *n16550)
{
        int stat = 0;

        n16550->iir = IIR_NO_IRQ;

        if ((n16550->ier & IER_LSR) && (n16550->int_status & SERIAL_INT_LSR)) /*Line status interrupt*/
        {
                stat = 1;
                n16550->iir = IIR_LSR;
        }
        else if ((n16550->ier & IER_RX) && (n16550->int_status & SERIAL_INT_RECEIVE)) /*Received data available*/
        {
                stat = 1;
                n16550->iir = IIR_RX;
        }
        else if ((n16550->ier & IER_TX) && (n16550->int_status & SERIAL_INT_TRANSMIT)) /*Transmit data empty*/
        {
                stat = 1;
                n16550->iir = IIR_TX;
        }
        else if ((n16550->ier & IER_MSR) && (n16550->int_status & SERIAL_INT_MSR)) /*Modem status interrupt*/
        {
                stat = 1;
                n16550->iir = IIR_MSR;
        }

//        lark_log("update_ints: %i %02x\n", stat, n16550->mctrl);
        n16550->set_irq(n16550->p, stat);
}

uint8_t n16550_read(n16550_t *n16550, uint32_t addr)
{
        uint8_t temp = 0xff;
        
        switch (addr & 7)
        {
                case 0:
                if (n16550->lcr & 0x80)
                {
                        temp = n16550->dlab1;
                        break;
                }

                n16550->lsr &= ~LSR_RX_READY;
                n16550->int_status &= ~SERIAL_INT_RECEIVE;
                update_ints(n16550);
                temp = n16550->rx_data;
//                lark_log("n16550_read: temp=%02x\n", temp);
                break;
                case 1:
                if (n16550->lcr & 0x80)
                        temp = n16550->dlab2;
                else
                        temp = n16550->ier;
                break;
                case 2:
                temp = n16550->iir;
                if ((temp & 0xe) == IIR_TX)
                {
                        n16550->int_status &= ~SERIAL_INT_TRANSMIT;
                        update_ints(n16550);
                }
                if (n16550->fcr & 1)
                        temp |= 0xc0;
                break;
                case 3:
                temp = n16550->lcr;
                break;
                case 4:
                temp = n16550->mctrl;
                break;
                case 5:
                if (n16550->lsr & 0x20)
                        n16550->lsr |= 0x40;
                n16550->lsr |= 0x20;
                temp = n16550->lsr;
                if (n16550->lsr & 0x1f)
                        n16550->lsr &= ~0x1e;
                n16550->int_status &= ~SERIAL_INT_LSR;
                update_ints(n16550);
                break;
                case 6:
                temp = n16550->msr;
                n16550->msr &= ~0x0f;
                n16550->int_status &= ~SERIAL_INT_MSR;
                update_ints(n16550);
                break;
                case 7:
                temp = n16550->scratch;
                break;
        }
        
        return temp;
}

void n16550_write(n16550_t *n16550, uint32_t addr, uint8_t val)
{
//        if (n16550->log)
//                n16550->log("n16550_write: addr=%01x val=%02x\n", addr & 7, val);
        switch (addr & 7)
        {
                case 0:
                if (n16550->lcr & 0x80)
                {
                        n16550->dlab1 = val;
                        if (n16550->dlab1 | (n16550->dlab2 << 8))
                                n16550->baud_rate = n16550->input_clock / ((n16550->dlab1 | (n16550->dlab2 << 8)) * 16);
                        else
                                n16550->baud_rate = n16550->input_clock / (3 * 16);
//                        if (n16550->log)
//                                n16550->log("baud_rate = %i\n", n16550->baud_rate);
                        return;
                }
                n16550->thr = val;
                n16550->lsr &= ~LSR_TX_EMPTY;
                //n16550->int_status |= SERIAL_INT_TRANSMIT;
                //update_ints(n16550);
                n16550->tx_data(n16550->p, val);
                n16550->tx_irq_pending = (1000000 * 10) / n16550->baud_rate;
                break;
                case 1:
                if (n16550->lcr & 0x80)
                {
                        n16550->dlab2 = val;
                        if (n16550->dlab1 | (n16550->dlab2 << 8))
                                n16550->baud_rate = n16550->input_clock / ((n16550->dlab1 | (n16550->dlab2 << 8)) * 16);
                        else
                                n16550->baud_rate = n16550->input_clock / (3 * 16);
//                        if (n16550->log)
//                                n16550->log("baud_rate = %i\n", n16550->baud_rate);
                        return;
                }
                n16550->ier = val & 0xf;
                update_ints(n16550);
                break;
                case 2:
                n16550->fcr = val;
                break;
                case 3:
                n16550->lcr = val;
                break;
                case 4:
                n16550->mctrl = val;
                if (val & 0x10)
                {
                        uint8_t new_msr;

                        new_msr = (val & 0x0c) << 4;
                        new_msr |= (val & 0x02) ? 0x10: 0;
                        new_msr |= (val & 0x01) ? 0x20: 0;

                        if ((n16550->msr ^ new_msr) & 0x10)
                                new_msr |= 0x01;
                        if ((n16550->msr ^ new_msr) & 0x20)
                                new_msr |= 0x02;
                        if ((n16550->msr ^ new_msr) & 0x80)
                                new_msr |= 0x08;
                        if ((n16550->msr & 0x40) && !(new_msr & 0x40))
                                new_msr |= 0x04;

                        n16550->msr = new_msr;
                }
                break;
                case 5:
                n16550->lsr = val;
                if (n16550->lsr & LSR_RX_READY)
                        n16550->int_status |= SERIAL_INT_RECEIVE;
                if (n16550->lsr & 0x1e)
                        n16550->int_status |= SERIAL_INT_LSR;
                if (n16550->lsr & LSR_TX_EMPTY)
                        n16550->int_status |= SERIAL_INT_TRANSMIT;
                update_ints(n16550);
                break;
                case 6:
                n16550->msr = val;
                if (n16550->msr & 0x0f)
                        n16550->int_status |= SERIAL_INT_MSR;
                update_ints(n16550);
                break;
                case 7:
                n16550->scratch = val;
                break;
        }
}

void n16550_run(n16550_t *n16550, int timeslice_us)
{
        if (n16550->tx_irq_pending)
        {
                n16550->tx_irq_pending -= timeslice_us;
                if (n16550->tx_irq_pending <= 0)
                {
//                        lark_log("n16550_write: tx irq\n");
                        n16550->tx_irq_pending = 0;
                        n16550->lsr |= LSR_TX_EMPTY;
                        n16550->int_status |= SERIAL_INT_TRANSMIT;
                        update_ints(n16550);
                }
        }
        if (n16550->rx_pending)
        {
                n16550->rx_pending -= timeslice_us;
                if (n16550->rx_pending <= 0)
                        n16550->rx_pending = 0;
        }
        if (!n16550->rx_pending && n16550->rx_rp != n16550->rx_wp)
        {
                n16550->rx_data = n16550->rx_queue[n16550->rx_rp & 0xff];
                n16550->rx_rp++;

                n16550->lsr |= LSR_RX_READY;
                n16550->int_status |= SERIAL_INT_RECEIVE;
                update_ints(n16550);

                n16550->rx_pending = (1000000 * 10) / n16550->baud_rate;
        }
}

void n16550_receive(n16550_t *n16550, uint8_t val)
{
        if ((n16550->rx_wp - n16550->rx_rp) >= 256)
        {
//                lark_log("n16550 drop %02x\n", val);
                return;
        }

//        lark_log("n16550 receive %02x %i %i\n", val, n16550->rx_wp, n16550->rx_rp);
        n16550->rx_queue[n16550->rx_wp & 0xff] = val;
        n16550->rx_wp++;
}
