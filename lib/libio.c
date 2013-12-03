#include "libcflat.h"
#include "libio.h"

void read_len(const volatile void *addr, void *buf, unsigned len)
{
	unsigned long val;

	switch (len) {
	case 1:
		val = readb(addr);
		break;
	case 2:
		val = readw(addr);
		break;
	case 4:
		val = readl(addr);
		break;
#ifdef CONFIG_64BIT
	case 8:
		val = readq(addr);
		break;
#endif
	default:
	{
		u8 *p = buf;
		unsigned i;

		for (i = 0; i < len; ++i)
			p[i] = readb(addr + i);
		return;
	}
	}
	memcpy(buf, &val, len);
}

void write_len(volatile void *addr, const void *buf, unsigned len)
{
	unsigned long val;

	if (len <= sizeof(unsigned long))
		memcpy(&val, buf, len);

	switch (len) {
	case 1:
		writeb(val, addr);
		break;
	case 2:
		writew(val, addr);
		break;
	case 4:
		writel(val, addr);
		break;
#ifdef CONFIG_64BIT
	case 8:
		writeq(val, addr);
		break;
#endif
	default:
	{
		const u8 *p = buf;
		unsigned i;

		for (i = 0; i < len; ++i)
			writeb(p[i], addr + i);
	}
	}
}
