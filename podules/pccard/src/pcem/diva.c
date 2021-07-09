#include "ibm.h"
#include "io.h"
#include "mem.h"
#include "pic.h"
#include "scamp.h"
#include "diva.h"
#include "x86.h"
#include "../libco/libco.h"

enum
{
	STATE_UNHALTED,
	STATE_HALTED_READ,
	STATE_HALTED_WRITE
};

static struct
{
	mem_mapping_t mapping;
	mem_mapping_t rom_mapping;

	uint8_t iss;
	uint16_t isc;
	uint16_t maddr;
	uint16_t mdata;

	int state;
	int is_running;

	cothread_t thread_386;
	cothread_t thread_main;
} diva;

#define ISS_READ  (1 << 4)
#define ISS_WORD  (1 << 6)
#define ISS_VALID (1 << 7)

#define ISC_POWERGOOD (1 << 0)
#define ISC_HISPEED   (1 << 1)

static const int irq_mapping[6] = {1, 8, 3, 6, 5, 15};
/*	bit 6 - raise IRQ1 (keyboard)
	bit 7 - raise IRQ8 (RTC)
	bit 8 - raise IRQ3 (COM2)
	bit 9 - raise IRQ6 (floppy)
	bit 10 - raise IRQ5 (LPT2)
	bit 11 - raise IRQ15*/

static void recalc_running_state(void)
{
	diva.is_running = (diva.isc & ISC_POWERGOOD) && (diva.state == STATE_UNHALTED);
//	pclog("recalc_running_state: is_running=%i\n", diva.is_running);
}

static void recalc_irqs(uint16_t val)
{
	int c;

	for (c = 6; c < 12; c++)
	{
		if ((val ^ diva.isc) & (1 << c))
		{
			if (val & (1 << c))
			{
//				pclog("IRQ raise %i\n", irq_mapping[c-6]);
				picintlevel(1 << irq_mapping[c-6]);
			}
			else
			{
//				pclog("IRQ clear %i\n", irq_mapping[c-6]);
				picintc(1 << irq_mapping[c-6]);
			}
		}
	}
}

int diva_run(int timeslice_us)
{
	if (diva.is_running)
	{
		cycles += timeslice_us * ((cpu_pccard_cpu == PCCARD_CPU_486SXLC2) ? 50 : 25);
		co_switch(diva.thread_386);
	}
	else if (diva.isc & ISC_POWERGOOD)
	{
		/*Ensure time spent waiting for ARM to respond isn't lost, so PIT timers tick at correct rate*/
		tsc += timeslice_us * ((cpu_pccard_cpu == PCCARD_CPU_486SXLC2) ? 50 : 25);
	}
	return 1;
}

static void diva_exec_386(void)
{
	while (1)
	{
		//pclog("diva_exec_386\n");
		exec386(0);
		co_switch(diva.thread_main);
	}
}

/*Should only ever be called from 386 thread*/
static void diva_halt(uint32_t addr, uint16_t val, uint8_t status)
{
	//pclog("diva_halt: addr=%08x val=%04x status=%02x\n", addr, val, status);
	if (!(status & ISS_READ))
		diva.mdata = val;
	diva.maddr = addr & 0xffff;
	diva.iss = (addr >> 16) | status | ISS_VALID;
	diva.state = (status & ISS_READ) ? STATE_HALTED_READ : STATE_HALTED_WRITE;
	recalc_running_state();
	cycles = 0;
	co_switch(diva.thread_main);
}

uint16_t diva_arm_read(uint32_t addr)
{
	uint16_t ret = 0xffff;

	switch (addr & 0x30)
	{
		case 0x00: /*ISS*/
		ret = diva.iss;
		break;

		case 0x10: /*ISC*/
		break;

		case 0x20: /*Mdata*/
		ret = diva.mdata;
//		pclog("  read mdata=%04x\n", ret);
		if (diva.state == STATE_HALTED_WRITE)
		{
			diva.iss &= ~ISS_VALID;
			diva.state = STATE_UNHALTED;
			recalc_running_state();
		}
		break;

		case 0x30: /*Maddr*/
		ret = diva.maddr;
//		pclog("  read maddr=%04x\n", ret);
		break;
	}

	//pclog("diva_arm_read: addr=%04x ret=%04x\n", addr, ret);
	return ret;
}
void diva_arm_write(uint32_t addr, uint16_t val)
{
	//pclog("diva_arm_write: addr=%04x val=%04x\n", addr, val);

	switch (addr & 0x30)
	{
		case 0x10: /*ISC*/
//		pclog("Write ISC %04x\n", val);
		if ((val & ISC_POWERGOOD) && !(diva.isc & ISC_POWERGOOD))
		{
			/*Reset & start running*/
			pclog("  386 reset!\n");
			resetx86();
			scamp_reset();
			if (diva.thread_386)
				fatal("Reset but thread already exists\n");
			diva.thread_386 = co_create(1024 * 1024, diva_exec_386);
			cycles = 0;
		}
		if (!(val & ISC_POWERGOOD) && diva.thread_386)
		{
			co_delete(diva.thread_386);
			diva.thread_386 = NULL;
		}
		recalc_irqs(val);
		diva.isc = val;
		recalc_running_state();
		break;

		case 0x20: /*Mdata*/
		diva.mdata = val;
		if (diva.state == STATE_HALTED_READ)
		{
			diva.iss &= ~ISS_VALID;
			diva.state = STATE_UNHALTED;
			recalc_running_state();
		}
		break;

		case 0x30: /*Maddr*/
		break;
	}
}


static uint8_t diva_in_b(uint16_t port, void *p)
{
	//fatal("diva_in_b: port=%08x\n", port);
	diva_halt(port | 0x10000, 0, ISS_READ);
	return diva.mdata;
}
static uint16_t diva_in_w(uint16_t port, void *p)
{
	//fatal("diva_in_w: port=%08x\n", port);
	diva_halt(port | 0x10000, 0, ISS_READ | ISS_WORD);
	return diva.mdata;
}

static void diva_out_b(uint16_t port, uint8_t val, void *p)
{
	//fatal("diva_out_b: port=%08x\n", port);
	diva_halt(port | 0x10000, (port & 1) ? (val << 8) : val, 0);
}
static void diva_out_w(uint16_t port, uint16_t val, void *p)
{
	//fatal("diva_out_w: port=%08x\n", port);
	diva_halt(port | 0x10000, val, ISS_WORD);
}

static uint8_t diva_read_b(uint32_t addr, void *p)
{
//	fatal("diva_read_b: addr=%08x\n", addr);
	diva_halt(addr, 0, ISS_READ);
	return diva.mdata;
}
static uint16_t diva_read_w(uint32_t addr, void *p)
{
	//fatal("diva_read_w: addr=%08x\n", addr);
	diva_halt(addr, 0, ISS_READ | ISS_WORD);
	return diva.mdata;
}

static void diva_write_b(uint32_t addr, uint8_t val, void *p)
{
	//pclog("diva_write_b: addr=%08x val=%02x\n", addr, val);
	diva_halt(addr, (addr & 1) ? (val << 8) : val, 0);
}
static void diva_write_w(uint32_t addr, uint16_t val, void *p)
{
	//fatal("diva_write_w: addr=%08x\n", addr);
	diva_halt(addr, val, ISS_WORD);
}

/*static void diva_io_trap(uint16_t start, uint16_t end)
{
	io_sethandler(start, (end-start)+1, diva_in_b, diva_in_w, NULL, diva_out_b, diva_out_w, NULL, NULL);

}*/

void diva_init(void)
{
	/*Note - no exec in Diva memory area!*/
	mem_mapping_add(&diva.mapping, 0x0a0000, 0x050000,
			diva_read_b, diva_read_w, NULL,
			diva_write_b, diva_write_w, NULL,
			NULL, MEM_MAPPING_EXTERNAL, NULL);
	/*ROM area is only byte wide*/
	mem_mapping_add(&diva.rom_mapping, 0x0f0000, 0x010000,
			diva_read_b, NULL, NULL,
			diva_write_b, NULL, NULL,
			NULL, MEM_MAPPING_EXTERNAL, NULL);
	/*Mapped IO ranges in Diva-I card :
	  0000-000f - DMA 0-3
	  0020-0021 - PIC 1
	  0040-0043 - PIT
	  0080-008f - DMA page registers
	  0092      - SCAMP
	  00a0-00a1 - PIC 2
	  00c0-00df - DMA 4-7
	  00e8      - SCAMP
	  00ea-00ef - SCAMP
	  00f4-00f5 - SCAMP
	  00f9      - SCAMP
	  00fb      - SCAMP
	  0378-037a - LPT1
	  03f8-03ff - COM1
	*/
	/*diva_io_trap(0x0010, 0x001f);
	diva_io_trap(0x0022, 0x003f);
	diva_io_trap(0x0044, 0x007f);
	diva_io_trap(0x0090, 0x0091);
	diva_io_trap(0x0093, 0x009f);
	diva_io_trap(0x00a2, 0x00bf);
	diva_io_trap(0x00e0, 0x00e7);
	diva_io_trap(0x00e9, 0x00e9);
	diva_io_trap(0x00f0, 0x00f3);
	diva_io_trap(0x00f6, 0x00f8);
	diva_io_trap(0x00fa, 0x00fa);
	diva_io_trap(0x00fc, 0x0377);
	diva_io_trap(0x037b, 0x03f7);
	diva_io_trap(0x0400, 0xffff);*/

	/*All IO accesses appear to get trapped, regardless of whether there's actually a device on board
	  to respond or not*/
	io_sethandler(0x0000, 0xffff, diva_in_b, diva_in_w, NULL, diva_out_b, diva_out_w, NULL, NULL);

	diva.thread_main = co_active();
	diva.thread_386 = NULL;

	diva.state = STATE_UNHALTED;
	diva.is_running = 0;
	diva.isc = 0;
}
