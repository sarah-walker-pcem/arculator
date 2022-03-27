#ifndef _DLL_H_
#define _DLL_H_

#if BUILDING_DLL
# define DLLIMPORT __declspec (dllexport)
#else /* Not BUILDING_DLL */
# define DLLIMPORT __declspec (dllimport)
#endif /* Not BUILDING_DLL */

#include "podule_api.h"

void aeh54_log(const char *format, ...);
void aeh54_fatal(const char *format, ...);

extern const podule_callbacks_t *podule_callbacks;

#endif /* _DLL_H_ */
