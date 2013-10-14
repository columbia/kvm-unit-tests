#ifndef _ARM_SYSINFO_H_
#define _ARM_SYSINFO_H_
#include "libcflat.h"

struct tag_core {
	u32 flags;		/* bit 0 = read-only */
	u32 pagesize;
	u32 rootdev;
};

struct tag_mem32 {
	u32   size;
	u32   start;	/* physical start address */
};

extern u32 mach_type_id;
extern struct tag_core core;
extern struct tag_mem32 mem32;
#endif
