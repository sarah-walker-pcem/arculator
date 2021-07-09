#ifndef _DIVA_H_
#define _DIVA_H_

void diva_init(void);
int diva_run(int timeslice_us);

uint16_t diva_arm_read(uint32_t addr);
void diva_arm_write(uint32_t addr, uint16_t val);

enum
{
	PCCARD_CPU_386SX,
	PCCARD_CPU_486SLC,
	PCCARD_CPU_486SXLC2
};

void pc_init(int cpu_type, int fpu_present, int mem_size_mb);

#endif /*_DIVA_H_*/