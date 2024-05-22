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

static void test_vmxpacket_h(void **)
{
	vmxpacket_t pkt;

	// too small `midi` size
	assert_int_equal(pkt.dt_address_aligned(), 0);
	assert_int_equal(pkt.dt_address_packed(), 0);
	assert_int_equal(pkt.dt_size_packed(), 0);

	// enough `midi` size
	pkt.midi.resize(15);
	assert_int_equal(pkt.dt_address_aligned(), 0);
	assert_int_equal(pkt.dt_address_packed(), 0);
	assert_int_equal(pkt.dt_size_packed(), 0);

	pkt.midi[6] = 0x11; // RQ1
	pkt.midi[7] = 0x34;
	pkt.midi[8] = 0x56;
	pkt.midi[9] = 0x78;
	pkt.midi[10] = 0x19;
	pkt.midi[11] = 0x43;
	pkt.midi[12] = 0x65;
	pkt.midi[13] = 0x87;
	pkt.midi[14] = 0x29;
	assert_int_equal(pkt.dt_address_aligned(), 0x34567819);
	assert_int_equal(pkt.dt_address_packed(), 0x34 << 21 | 0x56 << 14 | 0x78 << 7 | 0x19);
	assert_int_equal(pkt.dt_size_packed(), 0x43 << 21 | 0x65 << 14 | 0x87 << 7 | 0x29);

	pkt.midi.resize(14);
	pkt.midi[6] = 0x12; // DT1
	assert_int_equal(pkt.dt_address_aligned(), 0x34567819);
	assert_int_equal(pkt.dt_address_packed(), 0x34 << 21 | 0x56 << 14 | 0x78 << 7 | 0x19);
	assert_int_equal(pkt.dt_size_packed(), 1);

	pkt.midi.resize(7);
	pkt.midi[6] = 0x11; // RQ1
	assert_int_equal(pkt.dt_size_packed(), 0);
}

int main()
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_parse_failures),
		cmocka_unit_test(test_vmxpacket_h),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
