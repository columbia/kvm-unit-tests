#ifndef _ARM_IO_H_
#define _ARM_IO_H_

#include "arm/compiler.h"

#define __iomem
#define __force
#define cpu_is_be cpu_is_be
extern bool cpu_is_be;

#define __bswap16 bswap16
static inline u16 bswap16(u16 val)
{
	u16 ret;
	asm volatile("rev16 %0, %1" : "=r" (ret) :  "r" (val));
	return ret;
}

#define __bswap32 bswap32
static inline u32 bswap32(u32 val)
{
	u32 ret;
	asm volatile("rev %0, %1" : "=r" (ret) :  "r" (val));
	return ret;
}

#define __raw_readb __raw_readb
static inline u8 __raw_readb(const volatile void __iomem *addr)
{
	u8 val;
	asm volatile("ldrb %1, %0"
		     : "+Qo" (*(volatile u8 __force *)addr),
		       "=r" (val));
	return val;
}

#define __raw_readw __raw_readw
static inline u16 __raw_readw(const volatile void __iomem *addr)
{
	u16 val;
	asm volatile("ldrh %1, %0"
		     : "+Q" (*(volatile u16 __force *)addr),
		       "=r" (val));
	return val;
}

#define __raw_readl __raw_readl
static inline u32 __raw_readl(const volatile void __iomem *addr)
{
	u32 val;
	asm volatile("ldr %1, %0"
		     : "+Qo" (*(volatile u32 __force *)addr),
		       "=r" (val));
	return val;
}

#define __raw_writeb __raw_writeb
static inline void __raw_writeb(u8 val, volatile void __iomem *addr)
{
	asm volatile("strb %1, %0"
		     : "+Qo" (*(volatile u8 __force *)addr)
		     : "r" (val));
}

#define __raw_writew __raw_writew
static inline void __raw_writew(u16 val, volatile void __iomem *addr)
{
	asm volatile("strh %1, %0"
		     : "+Q" (*(volatile u16 __force *)addr)
		     : "r" (val));
}

#define __raw_writel __raw_writel
static inline void __raw_writel(u32 val, volatile void __iomem *addr)
{
	asm volatile("str %1, %0"
		     : "+Qo" (*(volatile u32 __force *)addr)
		     : "r" (val));
}

#define wmb() dsb()
#define rmb() dsb()

#include "libio.h"
#endif
