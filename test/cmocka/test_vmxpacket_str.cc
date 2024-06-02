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

static void test_add_from_raw(void **)
{
	vmxpacket_t pkt;
	std::vector<uint8_t> v;

	// Do nothing if length is smaller than 8.
	assert_int_equal(pkt.add_from_raw(nullptr, 7), 0);

	// Fail if not 0x02-0x00.
	v = std::vector<uint8_t>({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
	assert_int_equal(pkt.add_from_raw(&v[0], v.size()), -1);

	v = std::vector<uint8_t>({0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
	assert_int_equal(pkt.add_from_raw(&v[0], v.size()), -1);

	// Expected length (v[2], v[3]) is bigger than the given length.
	v = std::vector<uint8_t>({0x02, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00});
	assert_int_equal(pkt.add_from_raw(&v[0], v.size()), 0);

	// Expected length (v[2], v[3]) is smaller than or equal to 8
	v = std::vector<uint8_t>({0x02, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00});
	assert_int_equal(pkt.add_from_raw(&v[0], v.size()), -1);

	// Expected length (v[2], v[3]) is not a multiple of 4
	v = std::vector<uint8_t>({0x02, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00});
	assert_int_equal(pkt.add_from_raw(&v[0], v.size()), -1);

	// v[4:7] should be all zero.
	v = std::vector<uint8_t>({0x02, 0x00, 0x00, 0x08, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
	assert_int_equal(pkt.add_from_raw(&v[0], v.size()), -1);
	v = std::vector<uint8_t>({0x02, 0x00, 0x00, 0x08, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
	assert_int_equal(pkt.add_from_raw(&v[0], v.size()), -1);
	v = std::vector<uint8_t>({0x02, 0x00, 0x00, 0x08, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00});
	assert_int_equal(pkt.add_from_raw(&v[0], v.size()), -1);
	v = std::vector<uint8_t>({0x02, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00});
	assert_int_equal(pkt.add_from_raw(&v[0], v.size()), -1);

	// Error: aux data mismatch, ignoring %#02x %#02x\n
	v = std::vector<uint8_t>({0x02, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x00, 0xA4, 0x00, 0x00, 0x00, 0xB7, 0x00, 0x00, 0x00});
	assert_int_equal(pkt.add_from_raw(&v[0], v.size()), 16);

	// Error: expecting MIDI data end but length mismatch
	v = std::vector<uint8_t>({0x02, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x07, 0x0a, 0x0b, 0x0c, 0x04, 0x0d, 0x0e, 0x0f});
	pkt.midi.clear();
	assert_int_equal(pkt.add_from_raw(&v[0], v.size()), 16);
	assert_int_equal(pkt.midi.size(), 6);
	assert_int_equal(pkt.midi[0], v[9]);
	assert_int_equal(pkt.midi[1], v[10]);
	assert_int_equal(pkt.midi[2], v[11]);
	assert_int_equal(pkt.midi[3], v[13]);
	assert_int_equal(pkt.midi[4], v[14]);
	assert_int_equal(pkt.midi[5], v[15]);

	// Error: unknown header of quartet
	v = std::vector<uint8_t>({0x02, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
	assert_int_equal(pkt.add_from_raw(&v[0], v.size()), 16);
}

static void test_parse_tcp_stream(void **)
{
	vmxpacket_t pkt;
	std::vector<uint8_t> v;

	// Not all bytes are received yet.
	v = std::vector<uint8_t>({0x02, 0x00, 0x00, 0x08});
	pkt.midi.clear();
	assert_int_equal(parse_tcp_stream(&pkt, &v[0], v.size()), 0);
	assert_int_equal(pkt.midi.size(), 0);

	// is_dt1 returns false because midi.size() < 7
	v = std::vector<uint8_t>({0x02, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00});
	pkt.midi.clear();
	assert_int_equal(parse_tcp_stream(&pkt, &v[0], v.size()), v.size());

	// is_dt1 returns false because the sequence is not F0h-41h-...-12h
	v = std::vector<uint8_t>({0x02, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00});
	pkt.midi.clear();
	assert_int_equal(parse_tcp_stream(&pkt, &v[0], v.size()), v.size());

	// is_dt1 returns true but EOX was not found
	v = std::vector<uint8_t>({0x02, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x04, 0xF0, 0x41, 0x00, 0x04, 0x00, 0x00, 0x00, 0x07, 0x12, 0x00, 0x00});
	pkt.midi.clear();
	assert_int_equal(parse_tcp_stream(&pkt, &v[0], v.size()), 0);

	// EOX follows in a separated packet
	std::vector<uint8_t> add({0x02, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0xF7});
	v.insert(v.end(), add.begin(), add.end());
	pkt.midi.clear();
	assert_int_equal(parse_tcp_stream(&pkt, &v[0], v.size()), v.size());

	// is_dt1 returns true
	v = std::vector<uint8_t>({0x02, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x04, 0xF0, 0x41, 0x00, 0x04, 0x00, 0x00, 0x00, 0x07, 0x12, 0x00, 0xF7});
	pkt.midi.clear();
	assert_int_equal(parse_tcp_stream(&pkt, &v[0], v.size()), v.size());

	// Other than 00h-00h-00h-00h nor 02h-...
	v = std::vector<uint8_t>({0x03, 0x00, 0x00, 0x00});
	assert_int_equal(parse_tcp_stream(&pkt, &v[0], v.size()), -1);
}

int main()
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_parse_failures),
		cmocka_unit_test(test_vmxpacket_h),
		cmocka_unit_test(test_add_from_raw),
		cmocka_unit_test(test_parse_tcp_stream),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
