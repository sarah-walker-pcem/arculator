#ifndef _DLL_H_
#define _DLL_H_

#if BUILDING_DLL
# define DLLIMPORT __declspec (dllexport)
#else /* Not BUILDING_DLL */
# define DLLIMPORT __declspec (dllimport)
#endif /* Not BUILDING_DLL */

void aka31_log(const char *format, ...);
extern uint8_t aka31_ram[0x10000];

void aka31_sbic_int();
void aka31_sbic_int_clear();
void aka31_tc_int();

#endif /* _DLL_H_ */
