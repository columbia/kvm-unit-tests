/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * This code is borrowed from the Linux kernel arch/arm/kernel/psci.h
 *
 * Copyright (C) 2012 ARM Limited
 * Copyright (C) 2013 Linaro
 *
 */
#define DEBUG 1

#include "libcflat.h"
#include "test_util.h"
#include "arm/sysinfo.h"
#include "arm/ptrace.h"
#include "arm/processor.h"
#include "arm/psci.h"
#include "arm/setup.h"
#include "heap.h"

__asm__(".arch_extension	virt");
#define	noinline	__attribute__((noinline))

/*
 * Don't try to bring up more than once CPU at a time!
 */
struct secondary_data secondary_data;

static int (*invoke_psci_fn)(u32, u32, u32, u32);
enum psci_function {
	PSCI_FN_CPU_SUSPEND,
	PSCI_FN_CPU_ON,
	PSCI_FN_CPU_OFF,
	PSCI_FN_MIGRATE,
	PSCI_FN_MAX,
};

struct psci_power_state {
	u16	id;
	u8	type;
	u8	affinity_level;
};

static u32 psci_function_id[PSCI_FN_MAX];

#define PSCI_RET_SUCCESS		0
#define PSCI_RET_EOPNOTSUPP		-1
#define PSCI_RET_EINVAL			-2
#define PSCI_RET_EPERM			-3

#define PSCI_POWER_STATE_ID_MASK	0xffff
#define PSCI_POWER_STATE_ID_SHIFT	0
#define PSCI_POWER_STATE_TYPE_MASK	0x1
#define PSCI_POWER_STATE_TYPE_SHIFT	16
#define PSCI_POWER_STATE_AFFL_MASK	0x3
#define PSCI_POWER_STATE_AFFL_SHIFT	24

static u32 psci_power_state_pack(struct psci_power_state state)
{
	return	((state.id & PSCI_POWER_STATE_ID_MASK)
			<< PSCI_POWER_STATE_ID_SHIFT)	|
		((state.type & PSCI_POWER_STATE_TYPE_MASK)
			<< PSCI_POWER_STATE_TYPE_SHIFT)	|
		((state.affinity_level & PSCI_POWER_STATE_AFFL_MASK)
			<< PSCI_POWER_STATE_AFFL_SHIFT);
}

/*
 * This is used to ensure the compiler did actually allocate the register we
 * asked it for some inline assembly sequences.  Apparently we can't trust
 * the compiler from one version to another so a bit of paranoia won't hurt.
 * This string is meant to be concatenated with the inline asm string and
 * will cause compilation to stop on mismatch.
 * (for details, see gcc PR 15089)
 */
#define __asmeq(x, y)  ".ifnc " x "," y " ; .err ; .endif\n\t"

/*
 * The following function is invoked via the invoke_psci_fn pointer
 * and will not be inlined.
 */
static noinline int __invoke_psci_fn_hvc(u32 function_id, u32 arg0, u32 arg1,
					 u32 arg2)
{
	asm volatile(
			__asmeq("%0", "r0")
			__asmeq("%1", "r1")
			__asmeq("%2", "r2")
			__asmeq("%3", "r3")
			"hvc #0"
		: "+r" (function_id)
		: "r" (arg0), "r" (arg1), "r" (arg2));

	return function_id;
}

int psci_cpu_off(struct psci_power_state state)
{
	int err;
	u32 fn, power_state;

	fn = psci_function_id[PSCI_FN_CPU_OFF];
	power_state = psci_power_state_pack(state);
	err = invoke_psci_fn(fn, power_state, 0, 0);
	return err;
}

extern void smp_entry(void);
int psci_cpu_on(unsigned long cpuid, unsigned long entry_point)
{
	int err;
	u32 fn;
	void *page;
	struct cpu_thread_info *thread_info;

	page = alloc_page();
	if (!page) {
		printf("cannot allocate smp stack page\n");
		exit(FAIL);
	}

	thread_info = (struct cpu_thread_info *)page;
	init_cpu_thread_info(thread_info, cpuid);

	secondary_data.stack = (long)page + PAGE_SIZE - sizeof(long);
	secondary_data.entry_point = entry_point;

	fn = psci_function_id[PSCI_FN_CPU_ON];
	err = invoke_psci_fn(fn, cpuid, (u32)smp_entry, 0);
	return err;
}

/* KVM PSCI interface */
#define KVM_PSCI_FN_BASE		0x95c1ba5e
#define KVM_PSCI_FN(n)			(KVM_PSCI_FN_BASE + (n))

#define KVM_PSCI_FN_CPU_SUSPEND		KVM_PSCI_FN(0)
#define KVM_PSCI_FN_CPU_OFF		KVM_PSCI_FN(1)
#define KVM_PSCI_FN_CPU_ON		KVM_PSCI_FN(2)
#define KVM_PSCI_FN_MIGRATE		KVM_PSCI_FN(3)

#define KVM_PSCI_RET_SUCCESS		0
#define KVM_PSCI_RET_NI			((unsigned long)-1)
#define KVM_PSCI_RET_INVAL		((unsigned long)-2)
#define KVM_PSCI_RET_DENIED		((unsigned long)-3)

int psci_init(void)
{
	invoke_psci_fn = __invoke_psci_fn_hvc;
	psci_function_id[PSCI_FN_CPU_ON] = KVM_PSCI_FN_CPU_ON;
	psci_function_id[PSCI_FN_CPU_OFF] = KVM_PSCI_FN_CPU_OFF;
	return 0;
}
