#ifndef _TEST_UTIL_H_
#define _TEST_UTIL_H_
#include "libcflat.h"

#define PASS 0
#define FAIL 1

#define pass(fmt...) printf("PASS: " fmt)
#define fail(fmt...) printf("FAIL: " fmt)

bool enough_args(int nargs, int needed);
bool check_u32(u32 val, int base, char *expected);
#endif
