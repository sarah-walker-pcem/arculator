/*
 * $Id: hostfs.h,v 1.1 2005/07/27 21:34:34 mhowkins Exp $
 */

#ifndef HOSTFS_H
#define HOSTFS_H

//#include "armdefs.h"
//#include "arc.h"

#define ARCEM_SWI_CHUNK    0x56ac0
#define ARCEM_SWI_SHUTDOWN  (ARCEM_SWI_CHUNK + 0)
#define ARCEM_SWI_HOSTFS    (ARCEM_SWI_CHUNK + 1)
#define ARCEM_SWI_DEBUG     (ARCEM_SWI_CHUNK + 2)
//#define ARCEM_SWI_NANOSLEEP (ARCEM_SWI_CHUNK + 3)	/* Reserved */
#define ARCEM_SWI_NETWORK   (ARCEM_SWI_CHUNK + 4)

typedef uint32_t ARMword;
typedef struct {
  uint32_t *Reg;
} ARMul_State;


extern void hostfs(ARMul_State *state);
extern void hostfs_init(void);
extern void hostfs_reset(void);

static inline uint32_t ARMul_LoadWordS(ARMul_State *state, ARMword address)
{
        return readmeml(address);
}
static inline uint8_t ARMul_LoadByte(ARMul_State *state, ARMword address)
{
        return readmemb(address);
}

static inline void ARMul_StoreWordS(ARMul_State *state, ARMword address, uint32_t data)
{
        writememl(address, data);
}
static inline void ARMul_StoreByte(ARMul_State *state, ARMword address, uint8_t data)
{
        writememb(address, data);
}

#define UNIMPLEMENTED(section, format, args...) do { } while (0)

typedef struct {
	uint64_t	size;		/**< Size of disk */
	uint64_t	free;		/**< Free space on disk */
} disk_info;

extern int path_disk_info(const char *path, disk_info *d);

#endif
