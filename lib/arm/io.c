#include "libcflat.h"
#include "libio.h"
#include "iomaps.h"
#include "virtio-testdev.h"

static volatile u8 *uart0_base;

void puts(const char *s)
{
	while (*s)
		*uart0_base = *s++;
}

void exit(int code)
{
	virtio_testdev_exit(code);
	halt(code);
}

void io_init_early(void)
{
	const struct iomap *m = iomaps_find_compatible("arm,pl011");
	if (!m)
		halt(ENXIO);
	uart0_base = (u8 *)compat_ptr(m->addrs[0]);
}

void io_init(void)
{
	virtio_testdev_init();
}
