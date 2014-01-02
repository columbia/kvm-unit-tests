#define DEBUG 1

#include "libcflat.h"
#include "test_util.h"
#include "arm/sysinfo.h"
#include "arm/ptrace.h"
#include "arm/processor.h"
#include "arm/asm-offsets.h"
#include "arm/psci.h"

__asm__(".arch_extension	virt");

volatile bool secondary_is_up = false;

static struct spinlock lock;
static volatile int counter = 0;

static void increment_test(void)
{
	int prev;

	prev = counter;
	counter++;
	if (prev != counter - 1) {
		printf("Unexpected counter value, spinlocks are broken\n");
		exit(FAIL);
	}
}

void secondary_start(void)
{
	int cpu_id = get_cpu_id();
	unsigned counter = (1 << 20);

	debug("cpu%d: I'm alive!!!\n", cpu_id);
	if (cpu_id != 1) {
		printf("Unexepcted CPUID, expected %d, got %d\n", 1, cpu_id);
		exit(FAIL);
	}
	secondary_is_up = true;

	while (counter--) {
		spin_lock(&lock);
		increment_test();
		spin_unlock(&lock);
	}

	debug("[cpu 1]: spinlocks appear stable\n");

	halt(0);
}

int main(int argc __unused, char **argv __unused)
{
	int ret;
	int cpu_id = get_cpu_id();
	unsigned counter = (1 << 20);

	debug("smptest starting...\n");

	if (cpu_id != 0) {
		printf("Unexepcted CPUID, expected %d, got %d\n", 0, cpu_id);
		return FAIL;
	}

	ret = psci_cpu_on(1, (long)secondary_start);
	if (ret) {
		printf("starting second CPU failed\n");
		return FAIL;
	}
	
	debug("waiting for secondary cpu...\n");
	while (!secondary_is_up);
	debug("secondary CPU started\n");

	while (counter--) {
		spin_lock(&lock);
		increment_test();
		spin_unlock(&lock);
	}

	debug("[cpu 0]: spinlocks appear stable\n");

	return PASS;
}
