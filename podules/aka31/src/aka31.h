#ifndef _DLL_H_
#define _DLL_H_

#if BUILDING_DLL
# define DLLIMPORT __declspec (dllexport)
#else /* Not BUILDING_DLL */
# define DLLIMPORT __declspec (dllimport)
#endif /* Not BUILDING_DLL */

#include "podule_api.h"

void aka31_log(const char *format, ...);
void fatal(const char *format, ...);

void aka31_sbic_int();
void aka31_sbic_int_clear();
void aka31_tc_int();

void aka31_write_ram(podule_t *podule, uint16_t addr, uint8_t val);
uint8_t aka31_read_ram(podule_t *podule, uint16_t addr);

extern const podule_callbacks_t *podule_callbacks;
extern char podule_path[512];

#endif /* _DLL_H_ */
