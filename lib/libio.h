#ifndef _LIBIO_H_
#define _LIBIO_H_
/*
 * Adapted from the Linux kernel's include/asm-generic/io.h and
 * arch/arm/include/asm/io.h
 */
#include "libcflat.h"

#ifdef __arm__
#include "arm/io.h"
#endif

typedef u32 compat_ptr_t;

static inline void *compat_ptr(compat_ptr_t ptr)
{
	return (void *)(unsigned long)ptr;
}

static inline compat_ptr_t ptr_to_compat(void *ptr)
{
	return (u32)(unsigned long)ptr;
}

#ifndef __raw_readb
static inline u8 __raw_readb(const volatile void *addr)
{
	return *(const volatile u8 *)addr;
}
#endif

#ifndef __raw_readw
static inline u16 __raw_readw(const volatile void *addr)
{
	return *(const volatile u16 *)addr;
}
#endif

#ifndef __raw_readl
static inline u32 __raw_readl(const volatile void *addr)
{
	return *(const volatile u32 *)addr;
}
#endif

#ifdef CONFIG_64BIT
#ifndef __raw_readq
static inline u64 __raw_readq(const volatile void *addr)
{
	return *(const volatile u64 *)addr;
}
#endif
#endif

#ifndef __raw_writeb
static inline void __raw_writeb(u8 b, volatile void *addr)
{
	*(volatile u8 *)addr = b;
}
#endif

#ifndef __raw_writew
static inline void __raw_writew(u16 b, volatile void *addr)
{
	*(volatile u16 *)addr = b;
}
#endif

#ifndef __raw_writel
static inline void __raw_writel(u32 b, volatile void *addr)
{
	*(volatile u32 *)addr = b;
}
#endif

#ifdef CONFIG_64BIT
#ifndef __raw_writeq
static inline void __raw_writeq(u64 b, volatile void *addr)
{
	*(volatile u64 *)addr = b;
}
#endif
#endif

#ifndef __bswap16
static inline u16 __bswap16(u16 x)
{
	return ((x >> 8) & 0xff) | ((x & 0xff) << 8);
}
#endif

#ifndef __bswap32
static inline u32 __bswap32(u32 x)
{
	return ((x & 0xff000000) >> 24) | ((x & 0x00ff0000) >>  8) |
	       ((x & 0x0000ff00) <<  8) | ((x & 0x000000ff) << 24);
}
#endif

#ifdef CONFIG_64BIT
#ifndef __bswap64
static inline u64 __bswap64(u64 x)
{
	return ((x & 0x00000000000000ffULL) << 56) |
	       ((x & 0x000000000000ff00ULL) << 40) |
	       ((x & 0x0000000000ff0000ULL) << 24) |
	       ((x & 0x00000000ff000000ULL) <<  8) |
	       ((x & 0x000000ff00000000ULL) >>  8) |
	       ((x & 0x0000ff0000000000ULL) >> 24) |
	       ((x & 0x00ff000000000000ULL) >> 40) |
	       ((x & 0xff00000000000000ULL) >> 56);
}
#endif
#endif

#ifndef cpu_is_be
#define cpu_is_be 0
#endif

#define le16_to_cpu(x) \
	({ u16 __r = cpu_is_be ? __bswap16(x) : (x); __r; })
#define cpu_to_le16 le16_to_cpu

#define le32_to_cpu(x) \
	({ u32 __r = cpu_is_be ? __bswap32(x) : (x); __r; })
#define cpu_to_le32 le32_to_cpu

#ifdef CONFIG_64BIT
#define le64_to_cpu \
	({ u64 __r = cpu_is_be ? __bswap64(x) : (x); __r; })
#define cpu_to_le64 le64_to_cpu
#endif

#define be16_to_cpu(x) \
	({ u16 __r = !cpu_is_be ? __bswap16(x) : (x); __r; })
#define cpu_to_be16 be16_to_cpu

#define be32_to_cpu(x) \
	({ u32 __r = !cpu_is_be ? __bswap32(x) : (x); __r; })
#define cpu_to_be32 be32_to_cpu

#ifdef CONFIG_64BIT
#define be64_to_cpu \
	({ u64 __r = !cpu_is_be ? __bswap64(x) : (x); __r; })
#define cpu_to_be64 be64_to_cpu
#endif

#ifndef rmb
#define rmb() do { } while (0)
#endif
#ifndef wmb
#define wmb() do { } while (0)
#endif

#define readb(addr) \
	({ u8 __r = __raw_readb(addr); rmb(); __r; })
#define readw(addr) \
	({ u16 __r = le16_to_cpu(__raw_readw(addr)); rmb(); __r; })
#define readl(addr) \
	({ u32 __r = le32_to_cpu(__raw_readl(addr)); rmb(); __r; })
#ifdef CONFIG_64BIT
#define readq(addr) \
	({ u64 __r = le64_to_cpu(__raw_readq(addr)); rmb(); __r; })
#endif

#define writeb(b, addr) \
	({ wmb(); __raw_writeb(b, addr); })
#define writew(b, addr) \
	({ wmb(); __raw_writew(cpu_to_le16(b), addr); })
#define writel(b, addr) \
	({ wmb(); __raw_writel(cpu_to_le32(b), addr); })
#ifdef CONFIG_64BIT
#define writeq(b, addr) \
	({ wmb(); __raw_writeq(cpu_to_le64(b), addr); })
#endif

extern void read_len(const volatile void *addr, void *buf, unsigned len);
extern void write_len(volatile void *addr, const void *buf, unsigned len);

#endif
