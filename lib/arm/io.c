#include "libcflat.h"
#include "libio.h"
#include "iomaps.h"
#include "arm/mmu.h"
#include "arm/processor.h"
#include "virtio-testdev.h"

static volatile u8 *uart0_base;

static struct spinlock uart_lock = { 0 };

static void __puts_early(const char *s)
{
	while (*s)
		writeb(*s++, uart0_base);
}

static void __puts(const char *s)
{
	spin_lock(&uart_lock);
	while (*s)
		writeb(*s++, uart0_base);
	spin_unlock(&uart_lock);
}

void puts(const char *s)
{
	if (!mmu_enabled())
		__puts_early(s);
	else
		__puts(s);
}

void exit(int code)
{
	virtio_testdev_exit(code);
	halt(code);
}

#define QEMU_MACH_VIRT_PL011_BASE 0x09000000

void io_init_early(void)
{
	const struct iomap *m = iomaps_find_compatible("arm,pl011");
	if (m) {
		uart0_base = (u8 *)compat_ptr(m->addrs[0]);
	} else {
		/* take a lucky guess... */
		uart0_base = (u8 *)QEMU_MACH_VIRT_PL011_BASE;
		printf("Estimated uart0_base, please fix iomaps\n");
	}
}

void io_init(void)
{
	virtio_testdev_init();
}
