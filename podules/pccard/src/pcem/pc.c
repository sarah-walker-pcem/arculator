#include "ibm.h"
#include "cpu.h"
#include "diva.h"
#include "dma.h"
#include "io.h"
#include "lpt.h"
#include "mem.h"
#include "pic.h"
#include "pit.h"
#include "scamp.h"
#include "serial.h"
#include "x86.h"

void pc_init(int cpu_type, int fpu_present, int mem_size_mb)
{
	mem_size = mem_size_mb * 1024;//4096;

	cpu_set(cpu_type, fpu_present);
	setpitclock((cpu_type == PCCARD_CPU_486SXLC2) ? 50000000 : 25000000);
	mem_init();
	mem_alloc();
	io_init();
	timer_reset();
	resetx86();

	scamp_init();
	pic_init();
	pic2_init();
	pit_init();
	dma_init();
	dma16_init();
	serial1_init(0x3f8, 4, 1);
	lpt1_init(0x378);
	diva_init();
}