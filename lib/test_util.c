#include "libcflat.h"
#include "test_util.h"

bool enough_args(int nargs, int needed)
{
	if (nargs >= needed)
		return true;

	fail("Not enough arguments.\n");
	return false;
}

/*
 * Typically one would compare val == strtoul(expected, endp, base),
 * but we don't have, nor at this point really need, strtoul, so we
 * convert val to a string instead. base can only be 10 or 16.
 */
bool check_u32(u32 val, int base, char *expected)
{
	char *fmt = base == 10 ? "%d" : "%x";
	char val_str[16];

	snprintf(val_str, 16, fmt, val);

	if (base == 16)
		while (*expected == '0' || *expected == 'x')
			++expected;

	if (strcmp(val_str, expected) == 0)
		return true;

	fail("expected %s, but have %s\n", expected, val_str);
	return false;
}
