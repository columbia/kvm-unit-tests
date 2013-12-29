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

void secondary_start(void)
{
	debug("I'm alive!!!\n");
	secondary_is_up = true;
	halt(0);
}

int main(int argc __unused, char **argv __unused)
{
	int ret;

	debug("smptest starting...\n");
	ret = psci_cpu_on(1, (long)secondary_start);
	if (ret) {
		printf("starting second CPU failed\n");
		return FAIL;
	}
	
	debug("waiting for secondary cpu...\n");
	while (!secondary_is_up);
	printf("secondary CPU started\n");

	return PASS;
}
