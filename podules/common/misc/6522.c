#include <stdint.h>
#include "6522.h"

#define INT_CA1    0x02
#define INT_CA2    0x01
#define INT_CB1    0x10
#define INT_CB2    0x08
#define INT_TIMER1 0x40
#define INT_TIMER2 0x20

#define		ORB     0x00
#define		ORA	0x01
#define		DDRB	0x02
#define		DDRA	0x03
#define		T1CL	0x04
#define		T1CH	0x05
#define		T1LL	0x06
#define		T1LH	0x07
#define		T2CL	0x08
#define		T2CH	0x09
#define		SR	0x0a
#define		ACR	0x0b
#define		PCR	0x0c
#define		IFR	0x0d
#define		IER	0x0e
#define		ORAnh   0x0f

void via6522_updateIFR(via6522_t *v)
{
	if ((v->ifr & 0x7F) & (v->ier & 0x7F))
	{
		v->ifr |= 0x80;
		v->set_irq(v->p, 1);
	}
	else
	{
		v->ifr &= ~0x80;
		v->set_irq(v->p, 0);
	}
}

void via6522_updatetimers(via6522_t *v, int time)
{
	v->t1c -= time;
	v->t2c -= time;
	if (v->t1c < -3)
	{
		while (v->t1c < -1)
		      v->t1c += v->t1l + 2;
		if (!v->t1hit)
		{
			v->ifr |= INT_TIMER1;
			via6522_updateIFR(v);
		}
		if ((v->acr & 0x80) && !v->t1hit) /*Output to PB7*/
			v->orb ^= 0x80;
		if (!(v->acr & 0x40))
			v->t1hit = 1;
	}
	if (!(v->acr & 0x20))
	{
		if (v->t2c < -1 && !v->t2hit)
		{
			if (!v->t2hit)
			{
				v->ifr |= INT_TIMER2;
				via6522_updateIFR(v);
			}
			v->t2hit=1;
		}
	}
}

void via6522_write(via6522_t *v, uint16_t addr, uint8_t val)
{
	switch (addr & 0xf)
	{
		case ORA:
		v->ifr &= ~INT_CA1;
		if ((v->pcr & 0xA) != 0x2) /*Not independent interrupt for CA2*/
		   v->ifr &= ~INT_CA2;
		via6522_updateIFR(v);

		if ((v->pcr & 0x0E) == 0x08) /*Handshake mode*/
		{
			v->set_ca2(v->p, 0);
			v->ca2 = 0;
		}
		if ((v->pcr & 0x0E) == 0x0A) /*Pulse mode*/
		{
			v->set_ca2(v->p, 0);
			v->set_ca2(v->p, 1);
			v->ca2 = 1;
		}

		case ORAnh:
		v->write_portA(v->p, (val & v->ddra) | ~v->ddra);
		v->ora=val;
		break;

		case ORB:
		v->ifr &= ~INT_CB1;
		if ((v->pcr & 0xA0) != 0x20) /*Not independent interrupt for CB2*/
		   v->ifr &= ~INT_CB2;
		via6522_updateIFR(v);

		v->write_portB(v->p, (val & v->ddrb) | ~v->ddrb);
		v->orb=val;

		if ((v->pcr & 0xE0) == 0x80) /*Handshake mode*/
		{
			v->set_cb2(v->p, 0);
			v->cb2 = 0;
		}
		if ((v->pcr & 0xE0) == 0xA0) /*Pulse mode*/
		{
			v->set_cb2(v->p, 0);
			v->set_cb2(v->p, 1);
			v->cb2 = 1;
		}
		break;

		case DDRA:
		v->ddra = val;
		v->write_portA(v->p, (v->ora & v->ddra) | ~v->ddra);
		break;
		case DDRB:
		v->ddrb = val;
		v->write_portB(v->p, (v->orb & v->ddrb) | ~v->ddrb);
		break;
		case ACR:
		v->acr  = val;
		break;
		case PCR:
		v->pcr  = val;

		if ((val & 0xE) == 0xC)
		{
			v->set_ca2(v->p, 0);
			v->ca2 = 0;
		}
		else if (val & 0x8)
		{
			v->set_ca2(v->p, 1);
			v->ca2 = 1;
		}

		if ((val & 0xE0) == 0xC0)
		{
			v->set_cb2(v->p, 0);
			v->cb2 = 0;
		}
		else if (val & 0x80)
		{
			v->set_cb2(v->p, 1);
			v->cb2 = 1;
		}
		break;
		case SR:
		v->sr   = val;
		break;
		case T1LL:
		case T1CL:
		v->t1l &= 0xff00;
		v->t1l |= val;
		break;
		case T1LH:
		v->t1l &= 0x00ff;
		v->t1l |= (val << 8);
		if (v->acr & 0x40)
		{
			v->ifr &= ~INT_TIMER1;
			via6522_updateIFR(v);
		}
		break;
		case T1CH:
		if ((v->acr & 0xC0) == 0x80)
			v->orb &= ~0x80; /*Lower PB7 for one-shot timer*/
		v->t1l &= 0x00ff;
		v->t1l |= (val << 8);
		v->t1c = v->t1l + 1;
		v->t1hit = 0;
		v->ifr &= ~INT_TIMER1;
		via6522_updateIFR(v);
		break;
		case T2CL:
		v->t2l &= 0xff00;
		v->t2l |= val;
		break;
		case T2CH:
		v->t2l &= 0x00ff;
		v->t2l |= (val << 8);
		v->t2c  = v->t2l + 1;
		v->ifr &= ~INT_TIMER2;
		via6522_updateIFR(v);
		v->t2hit=0;
		break;
		case IER:
		if (val & 0x80)
		   v->ier |=  (val&0x7F);
		else
		   v->ier &= ~(val&0x7F);
		via6522_updateIFR(v);
		break;
		case IFR:
		v->ifr &= ~(val & 0x7F);
		via6522_updateIFR(v);
		break;
	}
}

uint8_t via6522_read(via6522_t *v, uint16_t addr)
{
	uint8_t temp;
	switch (addr & 0xf)
	{
		case ORA:
		v->ifr &= ~INT_CA1;
		if ((v->pcr & 0xA) != 0x2) /*Not independent interrupt for CA2*/
		   v->ifr &= ~INT_CA2;
		via6522_updateIFR(v);
		case ORAnh:
		temp = v->ora & v->ddra;
		if (v->acr & 1)
		   temp |= (v->ira & ~v->ddra); /*Read latch*/
		else
		   temp |= (v->read_portA(v->p) & ~v->ddra); /*Read current port values*/
		return temp;

		case ORB:
		v->ifr &= ~INT_CB1;
		if ((v->pcr & 0xA0) != 0x20) /*Not independent interrupt for CB2*/
		   v->ifr &= ~INT_CB2;
		via6522_updateIFR(v);

		temp = v->orb & v->ddrb;
		if (v->acr & 2)
		   temp |= (v->irb & ~v->ddrb); /*Read latch*/
		else
		   temp |= (v->read_portB(v->p) & ~v->ddrb); /*Read current port values*/
		return temp;

		case DDRA:
		return v->ddra;

		case DDRB:
		return v->ddrb;

		case T1LL:
		return v->t1l & 0xff;

		case T1LH:
		return v->t1l >> 8;

		case T1CL:
		v->ifr &= ~INT_TIMER1;
		via6522_updateIFR(v);
		if (v->t1c < -1) /*Return 0xFF during reload*/
			return 0xff;
		return v->t1c & 0xFF;

		case T1CH:
		if (v->t1c < -1) /*Return 0xFF during reload*/
			return 0xff;
		return v->t1c >> 8;

		case T2CL:
		v->ifr &= ~INT_TIMER2;
		via6522_updateIFR(v);
		return v->t2c & 0xFF;

		case T2CH:
		return v->t2c >>8;

		case SR:
		return v->sr;

		case ACR:
		return v->acr;

		case PCR:
		return v->pcr;

		case IER:
		return v->ier | 0x80;

		case IFR:
		return v->ifr;
	}
	return 0xFE;
}

void via6522_set_ca1(via6522_t *v, int level)
{
	if (level == v->ca1)
		return;
	if (((v->pcr & 0x01) && level) || (!(v->pcr & 0x01) && !level))
	{
		if (v->acr & 0x01)
			v->ira = v->read_portA(v->p); /*Latch port A*/
		v->ifr |= INT_CA1;
		via6522_updateIFR(v);
		if ((v->pcr & 0x0C) == 0x08) /*Handshaking mode*/
		{
			v->ca2 = 1;
			v->set_ca2(v->p, 1);
		}
	}
	v->ca1 = level;
}

void via6522_set_ca2(via6522_t *v, int level)
{
	if (level == v->ca2)
		return;
	if (v->pcr & 0x08) /*Output mode*/
		return;
	if (((v->pcr & 0x04) && level) || (!(v->pcr & 0x04) && !level))
	{
		v->ifr |= INT_CA2;
		via6522_updateIFR(v);
	}
	v->ca2 = level;
}

void via6522_set_cb1(via6522_t *v, int level)
{
	if (level == v->cb1)
		return;
	if (((v->pcr & 0x10) && level) || (!(v->pcr & 0x10) && !level))
	{
		if (v->acr & 0x02)
			v->irb = v->read_portB(v->p); /*Latch port B*/
		v->ifr |= INT_CB1;
		via6522_updateIFR(v);
		if ((v->pcr & 0xC0) == 0x80) /*Handshaking mode*/
		{
			v->cb2 = 1;
			v->set_cb2(v->p, 1);
		}
	}
	v->cb1 = level;
}

void via6522_set_cb2(via6522_t *v, int level)
{
	if (level == v->cb2)
		return;
	if (v->pcr & 0x80) /*Output mode*/
		return;
	if (((v->pcr & 0x40) && level) || (!(v->pcr & 0x40) && !level))
	{
		v->ifr |= INT_CB2;
		via6522_updateIFR(v);
	}
	v->cb2 = level;
}


static uint8_t via_read_null(void *p)
{
	return 0xFF;
}

static void via_write_null(void *p, uint8_t val)
{
}

static void via_set_null(void *p, int level)
{
}

void via6522_init(via6522_t *v, void (*set_irq)(void *p, int state), void *p)
{
	v->ora   = v->orb   = 0xff;
	v->ddra  = v->ddrb  = 0xff;
	v->ifr   = v->ier   = 0;
	v->t1c   = v->t1l   = 0xffff;
	v->t2c   = v->t2l   = 0xffff;
	v->t1hit = v->t2hit = 1;
	v->acr   = v->pcr   = 0;

	v->read_portA  = v->read_portB  = via_read_null;
	v->write_portA = v->write_portB = via_write_null;

	v->set_ca1 = v->set_ca2 = v->set_cb1 = v->set_cb2 = via_set_null;

	v->set_irq = set_irq;
	v->p = p;
}
