#ifndef _ARM_PROCESSOR_H_
#define _ARM_PROCESSOR_H_
#include "libcflat.h"
#include "ptrace.h"

enum {
	EXCPTN_RST,
	EXCPTN_UND,
	EXCPTN_SVC,
	EXCPTN_PABT,
	EXCPTN_DABT,
	EXCPTN_ADDREXCPTN,
	EXCPTN_IRQ,
	EXCPTN_FIQ,
};

extern void handle_exception(u8 v, void (*func)(struct pt_regs *regs));
extern void show_regs(struct pt_regs *regs);

extern void start_usr(void (*func)(void));

static inline unsigned long get_cpsr(void)
{
	unsigned long cpsr;
	asm volatile("mrs %0, cpsr" : "=r" (cpsr));
	return cpsr;
}

struct cpu_thread_info {
	int cpu_id;
};

extern struct cpu_thread_info *get_cpu_thread_info(void);
extern int get_cpu_id(void);
extern void *get_sp(void);

struct spinlock {
	int v;
};

void spin_lock(struct spinlock *lock);
void spin_unlock(struct spinlock *lock);

#endif
