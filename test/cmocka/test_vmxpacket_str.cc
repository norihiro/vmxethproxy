#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
extern "C" {
#include <cmocka.h>
}

#include "vmxpacket.h"

static void test_parse_failures(void **)
{
	vmxpacket_t pkt;

	// Error: Cannot parse command in '%s'
	assert_int_equal(vmxpacket_from_string(&pkt, " ", 0), false);

	// DT1: Error: %s: Cannot parse address in '%s'
	assert_int_equal(vmxpacket_from_string(&pkt, "DT1 ", 0), false);

	// RQ1: Error: %s: Cannot parse address in '%s'
	assert_int_equal(vmxpacket_from_string(&pkt, "RQ1 ", 0), false);

	// RQ1: Error: %s: Cannot parse size in '%s'
	assert_int_equal(vmxpacket_from_string(&pkt, "RQ1 0Aa", 0), false);

	assert_int_equal(vmxpacket_from_string(&pkt, "undefined", 0), false);

	// failure at parse_hexint
	assert_int_equal(vmxpacket_from_string(&pkt, "RQ1 0x", 0), false);
	assert_int_equal(vmxpacket_from_string(&pkt, "RQ1 .", 0), false);
}

int main()
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_parse_failures),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
