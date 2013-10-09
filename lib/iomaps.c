#include "libcflat.h"
#include "iomaps.h"

extern const struct iomap iomaps[];

const struct iomap *iomaps_find_type(const char *type)
{
	const struct iomap *m = &iomaps[0];

	while (m->type) {
		if (strcmp(m->type, type) == 0)
			return m;
		++m;
	}
	return NULL;
}

const struct iomap *iomaps_find_compatible(const char *compat)
{
	const struct iomap *m = &iomaps[0];
	const char *c;
	int i;

	while (m->type) {
		for (i = 0, c = m->compats[0]; c != NULL; c = m->compats[++i])
			if (strcmp(c, compat) == 0)
				return m;
		++m;
	}
	return NULL;
}
