#include "libcflat.h"
#include "test_util.h"
#include "arm/sysinfo.h"

int main(int argc, char **argv)
{
	int ret = FAIL;

	if (argc >= 1) {
		--argc;
		if (!strcmp(argv[0], "mem") && enough_args(argc, 1)) {
			if (check_u32(mem32.size/1024/1024, 10, argv[1]))
				ret = PASS;
		}
	}
	return ret;
}
