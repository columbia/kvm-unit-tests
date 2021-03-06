#include "libcflat.h"
#include "smp.h"
#include "io.h"
#ifndef USE_SERIAL
#define USE_SERIAL
#endif

static struct spinlock lock;
static int serial_iobase = 0x3f8;
static int serial_inited = 0;

static void serial_outb(char ch)
{
        u8 lsr;

        do {
                lsr = inb(serial_iobase + 0x05);
        } while (!(lsr & 0x20));

        outb(ch, serial_iobase + 0x00);
}

static void serial_init(void)
{
        u8 lcr;

        /* set DLAB */
        lcr = inb(serial_iobase + 0x03);
        lcr |= 0x80;
        outb(lcr, serial_iobase + 0x03);

        /* set baud rate to 115200 */
        outb(0x01, serial_iobase + 0x00);
        outb(0x00, serial_iobase + 0x01);

        /* clear DLAB */
        lcr = inb(serial_iobase + 0x03);
        lcr &= ~0x80;
        outb(lcr, serial_iobase + 0x03);
}

static void print_serial(const char *buf)
{
	unsigned long len = strlen(buf);
#ifdef USE_SERIAL
        unsigned long i;
        if (!serial_inited) {
            serial_init();
        }

        for (i = 0; i < len; i++) {
            serial_outb(buf[i]);
        }
#else
        asm volatile ("rep/outsb" : "+S"(buf), "+c"(len) : "d"(0xf1));
#endif
}

void puts(const char *s)
{
	spin_lock(&lock);
	print_serial(s);
	spin_unlock(&lock);
}

void exit(int code)
{
#ifdef USE_SERIAL
        static const char shutdown_str[8] = "Shutdown";
        int i;

        /* test device exit (with status) */
        outl(code, 0xf4);

        /* if that failed, try the Bochs poweroff port */
        for (i = 0; i < 8; i++) {
                outb(shutdown_str[i], 0x8900);
        }
#else
        asm volatile("out %0, %1" : : "a"(code), "d"((short)0xf4));
#endif
}
