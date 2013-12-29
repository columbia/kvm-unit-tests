#define DEBUG 1

#include "libcflat.h"
#include "test_util.h"
#include "arm/sysinfo.h"
#include "arm/ptrace.h"
#include "arm/processor.h"
#include "arm/asm-offsets.h"

__asm__(".arch_extension	virt");

#define GOAL (1ULL << 28)

#define ARR_SIZE(_x) ((int)(sizeof(_x) / sizeof(_x[0])))
#define for_each_test(_iter, _tests, _tmp) \
	for (_tmp = 0, _iter = _tests; \
	     _tmp < ARR_SIZE(_tests); \
	     _tmp++, _iter++)

static bool undef_happened = false;

struct exit_test {
	char *name;
	void (*test_fn)(void);
	int (*init_fn)(void);
	bool run;
};

static void und_handler(struct pt_regs *regs __unused)
{
	printf("cycle counter causes undefined exception\n");
	undef_happened = true;
}

static unsigned long read_cc(void)
{
	unsigned long cc;
	asm volatile("mrc p15, 0, %[reg], c9, c13, 0": [reg] "=r" (cc));
	return cc;
}

static void loop_test(struct exit_test *test)
{
	unsigned long i, iterations = 32;
	unsigned long c2, c1, cycles = 0;

	do {
		iterations *= 2;

		c1 = read_cc();
		for (i = 0; i < iterations; i++)
			test->test_fn();
		c2 = read_cc();

		if (c1 >= c2)
			continue;
		cycles = c2 - c1;
	} while (cycles < GOAL);

	debug("%s exit %d cycles over %d iterations = %d\n",
	       test->name, cycles, iterations, cycles / iterations);
	printf("%s\t%d\n",
	       test->name, cycles / iterations);
}

static void hvc_test(void)
{
	asm volatile("hvc #0");
}

static void noop_guest(void)
{
}

static struct exit_test available_tests[] = {
	{ "hvc",	hvc_test,	NULL,	false },
	{ "noop_guest",	noop_guest,	NULL,	false },
};

static struct exit_test *find_test(char *name)
{
	struct exit_test *test;
	int i;

	for_each_test(test, available_tests, i) {
		if (strcmp(test->name, name) == 0)
			return test;
	}

	return NULL;
}

static void run_tests(void)
{
	struct exit_test *test;
	int i, ret;

	for_each_test(test, available_tests, i) {
		if (!test->run)
			continue;

		ret = 0;
		if (test->init_fn) {
			debug("%s init...\n", test->name);
			ret = test->init_fn();
			if (ret) {
				printf("test init failed: %s (%d)\n",
				       test->name, ret);
				continue;
			}
		}
		debug("running test %s...\n", test->name);
		loop_test(test);
	}
}

int main(int argc, char **argv)
{
	int ret = PASS;
	unsigned long cc1 = 0, cc2 = 0;
	struct exit_test *test;
	int i;

	debug("vmexit tests up\n");

	handle_exception(EXCPTN_UND, und_handler);
	cc1 = read_cc();
	cc2 = read_cc();
	if (undef_happened || cc1 == cc2) {
		printf("Cannot read functional cycle counter (%d, %d)\n",
		       cc1, cc2);
		return FAIL;
	}
	handle_exception(EXCPTN_UND, NULL);

	if (argc == 0) {
		debug("running all tests\n");
		for_each_test(test, available_tests, i)
			test->run = true;
	} else {
		while (argc--) {
			test = find_test(argv[0]);
			if (!test) {
				printf("unknown test: %s\n", argv[0]);
				return FAIL;
			}
			test->run = true;
			argv++;
		}
	}

	run_tests();

	return ret;
}
