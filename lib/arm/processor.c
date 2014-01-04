#include "libcflat.h"
#include "arm/sysinfo.h"
#include "arm/ptrace.h"
#include "processor.h"
#include "arm/setup.h"
#include "heap.h"

static const char *processor_modes[] = {
	"USER_26", "FIQ_26" , "IRQ_26" , "SVC_26" ,
	"UK4_26" , "UK5_26" , "UK6_26" , "UK7_26" ,
	"UK8_26" , "UK9_26" , "UK10_26", "UK11_26",
	"UK12_26", "UK13_26", "UK14_26", "UK15_26",
	"USER_32", "FIQ_32" , "IRQ_32" , "SVC_32" ,
	"UK4_32" , "UK5_32" , "UK6_32" , "ABT_32" ,
	"UK8_32" , "UK9_32" , "UK10_32", "UND_32" ,
	"UK12_32", "UK13_32", "UK14_32", "SYS_32"
};

static char *vector_names[] = {
	"rst", "und", "svc", "pabt", "dabt", "addrexcptn", "irq", "fiq"
};

void show_regs(struct pt_regs *regs)
{
	unsigned long flags;
	char buf[64];

	printf("pc : [<%08lx>]    lr : [<%08lx>]    psr: %08lx\n"
	       "sp : %08lx  ip : %08lx  fp : %08lx\n",
		regs->ARM_pc, regs->ARM_lr, regs->ARM_cpsr,
		regs->ARM_sp, regs->ARM_ip, regs->ARM_fp);
	printf("r10: %08lx  r9 : %08lx  r8 : %08lx\n",
		regs->ARM_r10, regs->ARM_r9, regs->ARM_r8);
	printf("r7 : %08lx  r6 : %08lx  r5 : %08lx  r4 : %08lx\n",
		regs->ARM_r7, regs->ARM_r6, regs->ARM_r5, regs->ARM_r4);
	printf("r3 : %08lx  r2 : %08lx  r1 : %08lx  r0 : %08lx\n",
		regs->ARM_r3, regs->ARM_r2, regs->ARM_r1, regs->ARM_r0);

	flags = regs->ARM_cpsr;
	buf[0] = flags & PSR_N_BIT ? 'N' : 'n';
	buf[1] = flags & PSR_Z_BIT ? 'Z' : 'z';
	buf[2] = flags & PSR_C_BIT ? 'C' : 'c';
	buf[3] = flags & PSR_V_BIT ? 'V' : 'v';
	buf[4] = '\0';

	printf("Flags: %s  IRQs o%s  FIQs o%s  Mode %s\n",
		buf, interrupts_enabled(regs) ? "n" : "ff",
		fast_interrupts_enabled(regs) ? "n" : "ff",
		processor_modes[processor_mode(regs)]);

	if (!user_mode(regs)) {
		unsigned int ctrl, transbase, dac;
		asm volatile(
			"mrc p15, 0, %0, c1, c0\n"
			"mrc p15, 0, %1, c2, c0\n"
			"mrc p15, 0, %2, c3, c0\n"
		: "=r" (ctrl), "=r" (transbase), "=r" (dac));
		printf("Control: %08x  Table: %08x  DAC: %08x\n",
			ctrl, transbase, dac);
	}
}

void handle_exception(u8 v, exception_fn func)
{
	struct cpu_thread_info *ti = get_cpu_thread_info();
	exception_fn *handlers = *ti->exception_handlers;
	if (v < EXCPTN_MAX)
		handlers[v] = func;
}

void do_handle_exception(u8 v, struct pt_regs *regs)
{
	struct cpu_thread_info *ti = get_cpu_thread_info();
	exception_fn *handlers = *ti->exception_handlers;

	if (v < EXCPTN_MAX && handlers[v]) {
		handlers[v](regs);
		return;
	}

	if (v < EXCPTN_MAX)
		printf("Unhandled exception %d (%s)\n", v, vector_names[v]);
	else
		printf("%s called with vector=%d\n", __func__, v);

	printf("Exception frame registers:\n");
	show_regs(regs);
	if (v == EXCPTN_DABT) {
		unsigned long far, fsr;

		asm volatile("mrc p15, 0, %0, c6, c0, 0": "=r" (far));
		asm volatile("mrc p15, 0, %0, c5, c0, 0": "=r" (fsr));

		printf("DFAR: %08lx    DFSR: %08lx\n", far, fsr);
	} else if (v == EXCPTN_PABT) {
		unsigned long far, fsr;

		asm volatile("mrc p15, 0, %0, c6, c0, 2": "=r" (far));
		asm volatile("mrc p15, 0, %0, c5, c0, 1": "=r" (fsr));

		printf("IFAR: %08lx    IFSR: %08lx\n", far, fsr);
	}
	exit(EINTR);
}

void start_usr(void (*func)(void))
{
	void *sp_usr = alloc_page() + PAGE_SIZE;
	int cpu_id = get_cpu_id();
	struct cpu_thread_info *ti;

	ti = (struct cpu_thread_info *)((long)(sp_usr-1) & PAGE_MASK);
	init_cpu_thread_info(ti, cpu_id);

	asm volatile(
		"mrs	r0, cpsr\n"
		"bic	r0, #" __stringify(MODE_MASK) "\n"
		"orr	r0, #" __stringify(USR_MODE) "\n"
		"msr	cpsr_c, r0\n"
		"mov	sp, %0\n"
		"mov	pc, %1\n"
	:: "r" (sp_usr), "r" (func) : "r0");
}

/*
 * We store the cpu_therad_info on the bottom of our (assumed 1-page) stack.
 */
struct cpu_thread_info *get_cpu_thread_info(void)
{
	register unsigned long sp asm ("sp");
	return (struct cpu_thread_info *)((sp-1) & PAGE_MASK);
}

int get_cpu_id(void)
{
	struct cpu_thread_info *thread_info = get_cpu_thread_info();
	return thread_info->cpu_id;
}

void *get_sp(void)
{
	register unsigned long sp asm ("sp");
	return (void *)sp;
}

void spin_lock(struct spinlock *lock)
{
	u32 val, fail;

	dmb();
	do {
		asm volatile(
		"1:	ldrex	%0, [%2]\n"
		"	teq	%0, #0\n"
		"	bne	1b\n"
		"	mov	%0, #1\n"
		"	strex	%1, %0, [%2]\n"
		: "=&r" (val), "=&r" (fail)
		: "r" (&lock->v)
		: "r4", "cc" );
	} while (fail);
	dmb();
}

void spin_unlock(struct spinlock *lock)
{
	lock->v = 0;
	dmb();
}

static unsigned long irqsave(void)
{
	unsigned long flags;
	asm volatile("mrs	%0, cpsr\n"
		     "cpsid	i\n"
		     : "=r" (flags)
		     :
		     : "memory", "cc");
	return flags;
}

static unsigned long irqrestore(unsigned long flags)
{
	asm volatile("mrs	%0, cpsr\n"
		     "cpsid	i\n"
		     :
		     : "r" (flags)
		     : "memory", "cc");
	return flags;
}

void spin_lock_irqsave(struct spinlock *lock, unsigned long *flags)
{
	u32 contended, fail;

	*flags = irqsave();
	dmb();
	do {
		irqrestore(*flags);
		*flags = irqsave();
		asm volatile(
		"	ldrex	%0, [%2]\n"
		"	teq	%0, #0\n"
		"	bne	1f\n"
		"	mov	%0, #1\n"
		"	strex	%1, %0, [%2]\n"
		"1:	mov	%0, #0\n"
		: "=&r" (contended), "=&r" (fail)
		: "r" (&lock->v)
		: "r4", "cc" );
	} while (contended || fail);
	dmb();
}

void spin_unlock_irqrestore(struct spinlock *lock, unsigned long flags)
{
	lock->v = 0;
	dmb();
	irqrestore(flags);
}
