#include <stdint.h>
#include "d7002c.h"
#include "joystick_api.h"

uint8_t d7002c_read(d7002c_t *d7002c, uint16_t addr)
{
	switch (addr & 3)
	{
		case 0:
		d7002c->set_irq(d7002c->p, 0);
		return d7002c->status;
		break;
		case 1:
		return d7002c->high;
		break;
		case 2:
		return d7002c->low;
		break;
	}
	return 0x40;
}

void d7002c_write(d7002c_t *d7002c, uint16_t addr, uint8_t val)
{
	if (!(addr & 3))
	{
		d7002c->latch  = val;
		d7002c->time   = 5;
		d7002c->status = (val & 0xF) | 0x80; /*Busy, converting*/
		d7002c->set_irq(d7002c->p, 0);
	}
}

static void d7002c_complete(d7002c_t *d7002c)
{
	int32_t val;

	switch (d7002c->status & 3)
	{
		case 0: val = (-joystick_state[0].axis[0]) + 32768; break;
		case 1: val = (-joystick_state[0].axis[1]) + 32768; break;
		case 2: val = (-joystick_state[1].axis[0]) + 32768; break;
		case 3: val = (-joystick_state[1].axis[1]) + 32768; break;
	}

	if (val > 65535)
		val = 65535;
	if (val < 0)
		val = 0;

	d7002c->status  = (d7002c->status & 0xF) | 0x40; /*Not busy, conversion complete*/
	d7002c->status |= (val & 0xC000) >> 10;
	d7002c->high    =  val >> 8;
	d7002c->low     =  val & 0xFF;
	d7002c->set_irq(d7002c->p, 1);
}

void d7002c_poll(d7002c_t *d7002c)
{
	if (d7002c->time)
	{
		d7002c->time--;
		if (!d7002c->time)
			d7002c_complete(d7002c);
	}
}

void d7002c_init(d7002c_t *d7002c, void (*set_irq)(void *p, int state), void *p)
{
	d7002c->set_irq = set_irq;
	d7002c->p = p;

	d7002c->status = 0x40;            /*Not busy, conversion complete*/
	d7002c->high = 0;
	d7002c->low = 0;
	d7002c->latch = 0;
	d7002c->time = 0;
}
