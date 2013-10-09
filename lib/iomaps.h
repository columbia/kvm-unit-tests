#ifndef _IOMAPS_H_
#define _IOMAPS_H_
#include "libcflat.h"

struct iomap {
	const char *type;
	const char *compats[5];
	u32 nr;
	u32 addrs[64];
};

const struct iomap *iomaps_find_type(const char *type);
const struct iomap *iomaps_find_compatible(const char *compat);
#endif
