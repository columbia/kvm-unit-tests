
#include "libcflat.h"
#include "smp.h"
#include "processor.h"
#include "atomic.h"

static void outb(unsigned short port, int val)
{
    asm volatile("outb %b0, %w1" : "=a"(val) : "Nd"(port));
}

static unsigned int inb(unsigned short port)
{
    unsigned int val;
    asm volatile("xorl %0, %0; inb %w1, %b0" : "=a"(val) : "Nd"(port));
    return val;
}

static unsigned int inl(unsigned short port)
{
    unsigned int val;
    asm volatile("inl %w1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

#define GOAL (1ull << 30)

static int nr_cpus;

#ifdef __x86_64__
#  define R "r"
#else
#  define R "e"
#endif

static void cpuid_test(void)
{
	asm volatile ("push %%"R "bx; cpuid; pop %%"R "bx"
		      : : : "eax", "ecx", "edx");
}

static void vmcall(void)
{
	unsigned long a = 0, b, c, d;

	asm volatile ("vmcall" : "+a"(a), "=b"(b), "=c"(c), "=d"(d));
}

#define MSR_TSC_ADJUST 0x3b
#define MSR_EFER 0xc0000080
#define EFER_NX_MASK            (1ull << 11)

#ifdef __x86_64__
static void mov_from_cr8(void)
{
	unsigned long cr8;

	asm volatile ("mov %%cr8, %0" : "=r"(cr8));
}

static void mov_to_cr8(void)
{
	unsigned long cr8 = 0;

	asm volatile ("mov %0, %%cr8" : : "r"(cr8));
}
#endif

static int is_smp(void)
{
	return cpu_count() > 1;
}

static void nop(void *junk)
{
}

static void ipi(void)
{
	on_cpu(1, nop, 0);
}

static void ipi_halt(void)
{
	unsigned long long t;

	on_cpu(1, nop, 0);
	t = rdtsc() + 2000;
	while (rdtsc() < t)
		;
}

static void inl_pmtimer(void)
{
    inl(0xb008);
}

static void inl_nop_qemu(void)
{
    inl(0x1234);
}

static void inl_nop_kernel(void)
{
    inb(0x4d0);
}

static void outl_elcr_kernel(void)
{
    outb(0x4d0, 0);
}

static void ple_round_robin(void)
{
	struct counter {
		volatile int n1;
		int n2;
	} __attribute__((aligned(64)));
	static struct counter counters[64] = { { -1, 0 } };
	int me = smp_id();
	int you;
	volatile struct counter *p = &counters[me];

	while (p->n1 == p->n2)
		asm volatile ("pause");

	p->n2 = p->n1;
	you = me + 1;
	if (you == nr_cpus)
		you = 0;
	++counters[you].n1;
}

static void rd_tsc_adjust_msr(void)
{
	rdmsr(MSR_TSC_ADJUST);
}

static void wr_tsc_adjust_msr(void)
{
	wrmsr(MSR_TSC_ADJUST, 0x0);
}

static struct test {
	void (*func)(void);
	const char *name;
	int (*valid)(void);
	int parallel;
} tests[] = {
	{ cpuid_test, "cpuid", .parallel = 1,  },
	{ vmcall, "vmcall", .parallel = 1, },
#ifdef __x86_64__
	{ mov_from_cr8, "mov_from_cr8", .parallel = 1, },
	{ mov_to_cr8, "mov_to_cr8" , .parallel = 1, },
#endif
	{ inl_pmtimer, "inl_from_pmtimer", .parallel = 1, },
	{ inl_nop_qemu, "inl_from_qemu", .parallel = 1 },
	{ inl_nop_kernel, "inl_from_kernel", .parallel = 1 },
	{ outl_elcr_kernel, "outl_to_kernel", .parallel = 1 },
	{ ipi, "ipi", is_smp, .parallel = 0, },
	{ ipi_halt, "ipi+halt", is_smp, .parallel = 0, },
	{ ple_round_robin, "ple-round-robin", .parallel = 1 },
	{ wr_tsc_adjust_msr, "wr_tsc_adjust_msr", .parallel = 1 },
	{ rd_tsc_adjust_msr, "rd_tsc_adjust_msr", .parallel = 1 },
};

unsigned iterations;
static atomic_t nr_cpus_done;

static void run_test(void *_func)
{
    int i;
    void (*func)(void) = _func;

    for (i = 0; i < iterations; ++i)
        func();

    atomic_inc(&nr_cpus_done);
}

extern volatile unsigned long eoi_count;

static void do_test(struct test *test)
{
	int i;
	unsigned long long t1, t2;
        void (*func)(void) = test->func;

        iterations = 32;

        if (test->valid && !test->valid()) {
		printf("%s (skipped)\n", test->name);
		return;
	}

	do {
		iterations *= 2;
		if (func == ipi)
			eoi_count = 0;
		t1 = rdtsc();

		if (!test->parallel) {
			for (i = 0; i < iterations; ++i)
				func();
		} else {
			atomic_set(&nr_cpus_done, 0);
			for (i = cpu_count(); i > 0; i--)
				on_cpu_async(i-1, run_test, func);
			while (atomic_read(&nr_cpus_done) < cpu_count())
				;
		}
		t2 = rdtsc();
	} while ((t2 - t1) < GOAL);
	printf("%s %d\n", test->name, (int)((t2 - t1) / iterations));
	if (func == ipi) {
		printf("eoi %d\n", eoi_count / iterations);
	}
}

static void enable_nx(void *junk)
{
	if (cpuid(0x80000001).d & (1 << 20))
		wrmsr(MSR_EFER, rdmsr(MSR_EFER) | EFER_NX_MASK);
}

bool test_wanted(struct test *test, char *wanted[], int nwanted)
{
	int i;

	if (!nwanted)
		return true;

	for (i = 0; i < nwanted; ++i)
		if (strcmp(wanted[i], test->name) == 0)
			return true;

	return false;
}

int main(int ac, char **av)
{
	int i;

	smp_init();
	nr_cpus = cpu_count();

	for (i = cpu_count(); i > 0; i--)
		on_cpu(i-1, enable_nx, 0);

	for (i = 0; i < ARRAY_SIZE(tests); ++i)
		if (test_wanted(&tests[i], av + 1, ac - 1))
			do_test(&tests[i]);

	return 0;
}
