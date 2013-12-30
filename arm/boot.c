#include "libcflat.h"
#include "test_util.h"
#include "arm/sysinfo.h"
#include "arm/ptrace.h"
#include "arm/processor.h"
#include "arm/asm-offsets.h"
#include "arm/mmu.h"

static struct pt_regs expected_regs;
/* NOTE: update clobber list if passed insns needs more than r0,r1 */
#define test_excptn(pre_insns, excptn_insn, post_insns)		\
	asm volatile(						\
		pre_insns "\n"					\
		"mov	r0, %0\n"				\
		"stmia	r0, { r0-lr }\n"			\
		"mrs	r1, cpsr\n"				\
		"str	r1, [r0, #" __stringify(S_PSR) "]\n"	\
		"mov	r1, #-1\n"				\
		"str	r1, [r0, #" __stringify(S_OLD_R0) "]\n"	\
		"add	r1, pc, #8\n"				\
		"str	r1, [r0, #" __stringify(S_R1) "]\n"	\
		"str	r1, [r0, #" __stringify(S_PC) "]\n"	\
		excptn_insn "\n"				\
		post_insns "\n"					\
	:: "r" (&expected_regs) : "r0", "r1")

#define svc_mode() ((get_cpsr() & MODE_MASK) == SVC_MODE)

static bool check_regs(struct pt_regs *regs)
{
	unsigned i;

	if (!svc_mode())
		return false;

	for (i = 0; i < ARRAY_SIZE(regs->uregs); ++i)
		if (regs->uregs[i] != expected_regs.uregs[i])
			return false;

	return true;
}

static bool und_works;
static void und_handler(struct pt_regs *regs)
{
	if (check_regs(regs))
		und_works = true;
}

static bool check_und(void)
{
	handle_exception(EXCPTN_UND, und_handler);

	/* issue an instruction to a coprocessor we don't have */
	test_excptn("", "mcr p2, 0, r0, c0, c0", "");

	handle_exception(EXCPTN_UND, NULL);

	return und_works;
}

static bool svc_works;
static void svc_handler(struct pt_regs *regs)
{
	u32 svc = *(u32 *)(regs->ARM_pc - 4) & 0xffffff;

	if (!user_mode(regs)) {
		/*
		 * When issuing an svc from supervisor mode lr_svc will
		 * get corrupted. So before issuing the svc, callers must
		 * always push it on the stack. We pushed it to offset 4.
		 */
		regs->ARM_lr = *(unsigned long *)(regs->ARM_sp + 4);
	}

	if (check_regs(regs) && svc == 123)
		svc_works = true;
}

static bool check_svc(void)
{
	handle_exception(EXCPTN_SVC, svc_handler);

	if (svc_mode()) {
		/*
		 * An svc from supervisor mode will corrupt lr_svc and
		 * spsr_svc. We need to save/restore them separately.
		 */
		test_excptn(
			"mrs	r0, spsr\n"
			"push	{ r0,lr }\n",
			"svc	#123\n",
			"pop	{ r0,lr }\n"
			"msr	spsr, r0\n"
		);
	} else {
		test_excptn("", "svc #123", "");
	}

	handle_exception(EXCPTN_SVC, NULL);

	return svc_works;
}

static void check_vectors(void)
{
	int ret = check_und() && check_svc() ? PASS : FAIL;
	exit(ret);
}

static int boottest_enable_mmu(void)
{
	enable_mmu();
	return PASS;
}

int main(int argc, char **argv)
{
	int ret = FAIL;

	if (!enough_args(argc, 1))
		return ret;

	if (strcmp(argv[0], "mem") == 0 && enough_args(argc, 2)) {

		if (check_u32(mem32.size/1024/1024, 10, argv[1]))
			ret = PASS;

	} else if (strcmp(argv[0], "vectors") == 0) {

		check_vectors(); /* doesn't return */

	} else if (strcmp(argv[0], "vectors_usr") == 0) {

		start_usr(check_vectors); /* doesn't return */

	} else if (strcmp(argv[0], "enable_mmu") == 0) {

		ret = boottest_enable_mmu();

	}

	return ret;
}
