#ifndef IO_H
#define IO_H

static inline void writeb(char val, unsigned long addr)
{
	asm volatile("strb %[val], [%[addr], #0]": :
		     [val] "r" (val),
		     [addr] "r" ((unsigned long)addr));
}

static inline void writel(unsigned long val, unsigned long addr)
{
	asm volatile("str %[val], [%[addr], #0]": :
		     [val] "r" (val),
		     [addr] "r" ((unsigned long)addr));
}

#endif
