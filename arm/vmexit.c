#define DEBUG 1

#include "libcflat.h"
#include "test_util.h"
#include "iomaps.h"
#include "arm/sysinfo.h"
#include "arm/ptrace.h"
#include "arm/processor.h"
#include "arm/asm-offsets.h"
#include "arm/compiler.h"
#include "arm/psci.h"
#include "libio.h"
#include "virtio.h"

__asm__(".arch_extension	virt");

#define GOAL (1ULL << 28)

#define ARR_SIZE(_x) ((int)(sizeof(_x) / sizeof(_x[0])))
#define for_each_test(_iter, _tests, _tmp) \
	for (_tmp = 0, _iter = _tests; \
	     _tmp < ARR_SIZE(_tests); \
	     _tmp++, _iter++)

/* Some ARM GIC defines: */
#define GICC_CTLR		0x00000000
#define GICC_PMR		0x00000004
#define GICC_IAR		0x0000000c
#define GICC_EOIR		0x00000010

#define GICD_CTLR		0x00000000
#define GICD_ISENABLE(_n)	(0x00000100 + ((_n / 32) * 4))
#define GICD_SGIR		0x00000f00
#define GICD_SPENDSGI		0x00000f20

#define IAR_CPUID(_iar)		((_iar >> 10) & 0x7)
#define IAR_IRQID(_iar)		((_iar >> 0) & 0x3ff)

#define MK_EOIR(_cpuid, _irqid)	((((_cpuid) & 0x7) << 10) | ((_irqid) & 0x3ff))

#define ISENABLE_IRQ(_irq)	(1UL << (_irq % 32))

#define SGI_SET_PENDING(_target_cpu, _source_cpu) \
	((1UL << _target_cpu) << (8 * _source_cpu))

#define SGIR_IRQ_MASK			((1UL << 4) - 1)
#define SGIR_NSATTR			(1UL << 15)
#define SGIR_CPU_TARGET_LIST_SHIFT	(16)

#define SGIR_FORMAT(_target_cpu, _irq_num) ( \
	((1UL << _target_cpu) << SGIR_CPU_TARGET_LIST_SHIFT) | \
	((_irq_num) & SGIR_IRQ_MASK) | \
	SGIR_NSATTR)

static bool count_cycles = true;
static const int sgi_irq = 1;
static bool undef_happened = false;
static void *vgic_dist_addr = NULL;
static void *vgic_cpu_addr = NULL;
static volatile bool second_cpu_up = false;
static volatile bool first_cpu_ack;
static volatile bool ipi_acked;
static volatile bool ipi_received;
static volatile bool ipi_ready;
static bool run_once;

struct exit_test {
	char *name;
	void (*test_fn)(void);
	int (*init_fn)(void);
	bool run;
};

static void wfi(void)
{
	asm volatile("wfi");
}

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
	unsigned long c2 = 0, c1 = 0, cycles = 0;
	bool ipi_test = false;

	if (strcmp(test->name, "ipi") == 0)
		ipi_test = true;

	do {
		iterations *= 2;

		/* wait for ipi to be ready */
		while (ipi_test && !ipi_ready);

		if (count_cycles)
			c1 = read_cc();
		for (i = 0; i < iterations; i++)
			test->test_fn();
		if (count_cycles)
			c2 = read_cc();

		ipi_ready = false;

		if (c1 >= c2 && count_cycles)
			continue;
		cycles = c2 - c1;
	} while (cycles < GOAL && count_cycles);

	debug("%s exit %d cycles over %d iterations = %d\n",
	       test->name, cycles, iterations, cycles / iterations);
	printf("%s\t%d\n",
	       test->name, cycles / iterations);
}

static void hvc_test(void)
{
	asm volatile("mov r0, #0x4b000000; hvc #0");
}

static void noop_guest(void)
{
}

static void *mmio_read_user_addr = NULL;
static int mmio_read_user_init(void)
{
	const struct iomap *m;

	if (mmio_read_user_addr)
		return 0;

	m = iomaps_find_compatible("virtio,mmio");
	if (!m) {
		printf("%s: No virtio-mmio transports found!\n", __func__);
		return EINVAL;
	}
	mmio_read_user_addr = (void *)(m->addrs[0] + VIRTIO_MMIO_DEVICE_ID);
	return 0;
}

static void mmio_read_user(void)
{
	readl(mmio_read_user_addr);
}

static int vgic_addr_init(void)
{
	const struct iomap *m;

	if (vgic_dist_addr)
		return 0;

	m = iomaps_find_compatible("arm,cortex-a15-gic");
	if (!m) {
		printf("%s: No GIC addresses found!\n", __func__);
		return EINVAL;
	}
	vgic_dist_addr = (void *)(m->addrs[0]);
	vgic_cpu_addr = (void *)(m->addrs[1]);

	debug("vgic_dist_addr: %x\n", vgic_dist_addr);
	debug("vgic_cpu_addr: %x\n", vgic_cpu_addr);

	return 0;
}

static int mmio_read_vgic_init(void)
{
	return vgic_addr_init();
}

static void mmio_read_vgic(void)
{
	readl(vgic_dist_addr + 0x8);
}

#define ipi_debug(fmt, ...) \
	debug("ipi_test [cpu %d]: " fmt, get_cpu_id(), ## __VA_ARGS__)

#define debug(fmt, ...) \
	do { printf(fmt , ## __VA_ARGS__); } while (0)
static void ipi_irq_handler(struct pt_regs *regs __unused)
{
	unsigned long ack, eoi;
	int irq, cpu;

	ipi_debug("received interrupt\n");

	ipi_received = true;

	ack = readl(vgic_cpu_addr + GICC_IAR);
	irq = IAR_IRQID(ack);
	cpu = IAR_CPUID(ack);

	ipi_acked = true;

	ipi_debug("received interrupt %u (src_cpu %u)\n", irq, cpu);

	if (irq != sgi_irq) {
		printf("unknown irq: %u\n", irq);
		exit(FAIL);
	}

	eoi = MK_EOIR(cpu, irq);
	writel(eoi, vgic_cpu_addr + GICC_EOIR);

	ipi_ready = true;
}

static void ipi_test_secondary_entry(void)
{
	unsigned int timeout = 1U << 28;

	ipi_debug("secondary core up\n");

	handle_exception(EXCPTN_IRQ, ipi_irq_handler);

	writel(0x1, vgic_cpu_addr + GICC_CTLR); /* enable cpu interface */
	writel(0xff, vgic_cpu_addr + GICC_PMR);	/* unmask all irq priorities */

	second_cpu_up = true;

	ipi_debug("secondary initialized vgic\n");

	while (!first_cpu_ack && timeout--);
	if (!first_cpu_ack) {
		printf("ipi_test: First CPU did not ack wake-up\n");
	}

	ipi_debug("detected first cpu ack\n");

	/* Enter small wait-loop */
	enable_interrupts();
	ipi_ready = true;
	while (true)
		wfi();
}

static int ipi_test_init(void)
{
	int ret;
	unsigned int timeout = 1U << 28;

	ipi_ready = false;

	ret = vgic_addr_init();
	if (ret)
		goto out;

	/* Enable distributor and SGI used for ipi test */
	writel(0x1, vgic_dist_addr + GICD_CTLR); /* enable distributor */
	writel(ISENABLE_IRQ(sgi_irq), vgic_dist_addr + GICD_ISENABLE(sgi_irq));

	ipi_debug("starting second CPU\n");
	ret = psci_cpu_on(1, (unsigned long)ipi_test_secondary_entry);
	if (ret)
		goto out;

	/* Wait for second CPU! */
	while (!second_cpu_up && timeout--);

	if (!second_cpu_up) {
		printf("ipi_test: timeout waiting for secondary CPU\n");
		return FAIL;
	}

	ipi_debug("detected secondary core up\n");

	first_cpu_ack = true;

out:
	return ret;
}

static void ipi_test(void)
{
	unsigned long val;
	unsigned int timeout = 1U << 24;

	while (!ipi_ready && timeout--);

	if (!ipi_ready) {
		printf("ipi_test: second core not ready for IPIs\n");
		exit(FAIL);
	}

	ipi_received = false;

	/* Signal IPI/SGI IRQ to CPU 1 */
	val = SGIR_FORMAT(1, sgi_irq);
	writel(val, vgic_dist_addr + GICD_SGIR);

	while (!ipi_received);
}

static struct exit_test available_tests[] = {
	{ "hvc",		hvc_test,	NULL,			false },
	{ "noop_guest",		noop_guest,	NULL,			false },
	{ "mmio_read_user",	mmio_read_user,	mmio_read_user_init,	false },
	{ "mmio_read_vgic",	mmio_read_vgic,	mmio_read_vgic_init,	false },
	{ "ipi",		ipi_test,	ipi_test_init,		false },
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
		if (run_once)
			test->test_fn();
		else
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
		count_cycles = false;
		return FAIL;
	}
	handle_exception(EXCPTN_UND, NULL);

	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "--oneshot") == 0) {
			run_once = true;
			argv[i] = argv[argc - 1];
			argc--;
			break;
		}
	}

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
