#include "libcflat.h"

void __flat_div0(void)
{
	printf("panic: Division by 0\n");
	exit(1);
}
