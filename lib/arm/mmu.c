#define DEBUG 1

#include "libcflat.h"
#include "test_util.h"
#include "arm/sysinfo.h"
#include "arm/ptrace.h"
#include "arm/processor.h"
#include "arm/asm-offsets.h"
#include "arm/psci.h"
#include "arm/setup.h"
#include "arm/compiler.h"
#include "heap.h"

#define MAX_CPUS	2

typedef unsigned long long u64;

/* Maximum capacity for two CPUs */
static u64 pgd_mem[4] __attribute__ ((aligned (32)));

#define PGD_SHIFT	 30
#define PGD_SIZE 	(1 << PGD_SHIFT)
#define PGD_AF   	(1 << 10) /* Don't raise access flag exceptions */
#define PGD_SH	 	(3 << 8) /* All memory inner+outer shareable */
#define PGD_TYPE_BLOCK	(1 << 0) /* All memory inner+outer shareable */
#define PGD_AP_SHIFT	(6)
#define PGD_AP_MASK	(0x3 << PGD_AP_SHIFT)
#define PGD_AP_PL0	(0x1 << PGD_AP_SHIFT)
#define PGD_AP_PL1	(0x0 << PGD_AP_SHIFT)

/*
 * Note that we do not mark virtio and vgic memory regions as device memory as
 * the Stage-2 mappings should overwrite this for the vgic device access and
 * for other devices there will be no Stage-2 entries and it doesn't matter
 * anyhow.  Ideally, we'd do something smarter.
 */

#define TTBCR_EAE	(0x1UL << 31)

#define SCTLR_M		(0x1 << 0)
#define SCTLR_C		(0x1 << 2)
#define SCTLR_I		(0x1 << 12)


static inline void set_ttbr1(unsigned long long value)
{
	unsigned long low = (value << 32) >> 32;
	unsigned long high = (value >> 32);

	asm volatile("mcrr p15, 1, %[val_l], %[val_h], c2": :
		     [val_l] "r" (low), [val_h] "r" (high));
	isb();
}

static inline void set_ttbr0(unsigned long long value)
{
	unsigned long low = (value << 32) >> 32;
	unsigned long high = (value >> 32);

	asm volatile("mcrr p15, 0, %[val_l], %[val_h], c2": :
		     [val_l] "r" (low), [val_h] "r" (high));
	isb();
}

static inline unsigned long long get_ttbr0()
{
	unsigned long low;
	unsigned long high;

	asm volatile("mrrc p15, 0, %[val_l], %[val_h], c2":
		     [val_l] "=r" (low), [val_h] "=r" (high));
	return ((unsigned long long)high << 32ULL) | (unsigned long long)low;
}

static inline void set_ttbcr(unsigned long value)
{
	asm volatile("mcr p15, 0, %[val], c2, c0, 2": :
		     [val] "r" (value));
	isb();
}

static inline unsigned long get_sctlr(void)
{
	unsigned long value;

	asm volatile("mrc p15, 0, %[val], c1, c0, 0":
		     [val] "=r" (value));
	return value;
}

static inline void set_sctlr(unsigned long value)
{
	asm volatile("mcr p15, 0, %[val], c1, c0, 0": :
		     [val] "r" (value));
	isb();
}

static inline unsigned long get_mair0(void)
{
	unsigned long value;

	asm volatile("mrc p15, 0, %[val], c10, c2, 0":
		     [val] "=r" (value));
	return value;
}

static inline void set_mair0(unsigned long value)
{
	asm volatile("mcr p15, 0, %[val], c10, c2, 0": :
		     [val] "r" (value));
	isb();
}

void enable_mmu(void)
{
	unsigned long long i;
	unsigned long ttbcr;
	unsigned long long ttbr0;
	unsigned long sctlr;
	unsigned long pgd_ptr;
	unsigned long mair0;
	u64 *pgd;
	int cpu = get_cpu_id();

	if (cpu >= MAX_CPUS) {
		printf("mmu enable not supported for more than %d cpus (cpu: %d)\n",
		       MAX_CPUS, cpu);
		exit(FAIL);

	}

	/* Set up an identitity map */
	pgd = pgd_mem;

	if (*pgd == 0) {
		debug("core %d: pgd: %p\n", cpu, pgd);
		for (i = 0; i < 4; i++) {
			pgd[i] = (i * PGD_SIZE);
			pgd[i] |= PGD_AF | PGD_SH | PGD_TYPE_BLOCK | PGD_AP_PL0;
		}
		dsb();
		dmb();
	} else {
		debug("core %d: pgd already configured\n", cpu, pgd);
	}

	ttbcr = TTBCR_EAE;
	set_ttbcr(ttbcr);

	/*
	 * MAIR0:
	 *  attr[0-3], normal memory, inner-outer cacheable,
	 *             read and write allocate
	 */
	mair0 = 0xffffffff;
	set_mair0(mair0);

	pgd_ptr = (unsigned long)pgd;
	ttbr0 = (unsigned long long)pgd_ptr & (~(0x1fULL));
	set_ttbr0(ttbr0);

	sctlr = get_sctlr();
	sctlr |= SCTLR_M | SCTLR_C | SCTLR_I;
	set_sctlr(sctlr);

	debug("core[%d]: mmu enabled! (0x%x)\n", cpu, get_sp());
}

bool mmu_enabled(void)
{
	unsigned long sctlr;
	sctlr = get_sctlr();
	return (sctlr & SCTLR_M);
}
