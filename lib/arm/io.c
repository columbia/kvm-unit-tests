#include "libcflat.h"
#include "io.h"

#define IO_SERIAL_OFFSET	0
#define IO_EXIT_OFFSET		42

static struct spinlock lock;
static int iobase = 0xff000000;

static void serial_writeb(char ch)
{
	writeb(ch, iobase + IO_SERIAL_OFFSET);
}

static void print_serial(const char *buf)
{
	unsigned int i;
	unsigned long len = strlen(buf);

	for (i = 0; i < len; i++) {
		serial_writeb(buf[i]);
	}
}

static void testdev_exit(int status)
{
	writel(status, iobase + IO_EXIT_OFFSET);
}

#define spin_lock(x) do { (void)x; } while (0)
#define spin_unlock(x) do { (void)x; } while (0)

void puts(const char *s)
{
	spin_lock(&lock);
	print_serial(s);
	spin_unlock(&lock);
}

void exit(int code)
{
	static const char *shutdown_str = "Shutdown";

	puts(shutdown_str);

	/* test device exit (with status) */
	testdev_exit(code);
}
