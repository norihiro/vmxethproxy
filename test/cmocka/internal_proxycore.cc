#include "proxycore.cc"

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
extern "C" {
#include <cmocka.h>
}
#include <unistd.h>
#include <sys/socket.h>

static vmxpacket_t *set_address(vmxpacket_t *p, int addr)
{
	std::vector<uint8_t> &midi = p->modify_midi();
	midi = std::vector<uint8_t>{
		0xF0,
		0x41,
		0x00, // device ID
		0x00,
		0x00,
		0x24,
		0x12, // DT1
		(uint8_t)((addr >> 24) & 0x7F),
		(uint8_t)((addr >> 16) & 0x7F), // address MSP
		(uint8_t)((addr >> 8) & 0x7F),
		(uint8_t)(addr & 0x7F), // address LSB
		0x00,                   // checksum
		0xF7,                   // EOX
	};
	p->make_raw();
	return p;
}

static void test_is_basic_address(void **)
{
	vmxpacket_t pkt;

	assert_int_equal(is_basic_address(set_address(&pkt, 0x00000000)), true);  // sampling freq
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000003)), true);  // Ch-mute option mutes direct outputs
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000004)), false); // reserved
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000007)), false); // reserved
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000008)), true);  // recorder play mode
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000009)), false); // reserved
	assert_int_equal(is_basic_address(set_address(&pkt, 0x1000000F)), false); // reserved
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000010)), true);  // meter over level
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000011)), true);
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000012)), true);  // meter peak hold time
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000013)), true);  // meter peak hold
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000014)), true);  // meter ch metering point
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000015)), true);  // meter bus metering point
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000016)), false); // reserved
	assert_int_equal(is_basic_address(set_address(&pkt, 0x1000001F)), false); // reserved
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000020)), true);  // date format
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000021)), false); // reserved
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000022)), true);  // delay unit
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000023)), true);  // panel brightness
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000024)), true);  // display brightness
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000025)), true);  // display contrast
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000026)), false); // reserved
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000027)), true);  // main mute
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000028)), true);  // GEQ 0.5dB step
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000029)), false); // reserved
	assert_int_equal(is_basic_address(set_address(&pkt, 0x1000007F)), false); // reserved
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000100)), true);  // MIDI out
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000101)), true);  // device ID
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000102)), true);  // RS-232C rate
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000103)), true);  // RS-232C/MIDI select
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000104)), false); // reserved
	assert_int_equal(is_basic_address(set_address(&pkt, 0x1000010F)), false); // reserved

	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000110)), true);  // MIDI control change Rx
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000111)), true);  // MIDI program change Rx
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000112)), true);  // MIDI SysEx Rx
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000113)), false); // reserved
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000114)), true);  // MIDI MMC Rx
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000115)), false); // reserved
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000117)), false); // reserved
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000118)), true);  // MIDI control change Tx
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000119)), true);  // MIDI program change Tx
	assert_int_equal(is_basic_address(set_address(&pkt, 0x1000011A)), true);  // MIDI SysEx Tx
	assert_int_equal(is_basic_address(set_address(&pkt, 0x1000011B)), false); // reserved
	assert_int_equal(is_basic_address(set_address(&pkt, 0x1000011F)), false); // reserved

	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000120)), true);  // USB MIDI control change Rx
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000121)), true);  // USB MIDI program change Rx
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000122)), true);  // USB MIDI SysEx Rx
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000123)), false); // reserved
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000124)), true);  // USB MIDI MMC Rx
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000125)), false); // reserved
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000127)), false); // reserved
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000128)), true);  // USB MIDI control change Tx
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000129)), true);  // USB MIDI program change Tx
	assert_int_equal(is_basic_address(set_address(&pkt, 0x1000012A)), true);  // USB MIDI SysEx Tx
	assert_int_equal(is_basic_address(set_address(&pkt, 0x1000012B)), false); // reserved
	assert_int_equal(is_basic_address(set_address(&pkt, 0x1000012F)), false); // reserved

	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000130)), false);
	assert_int_equal(is_basic_address(set_address(&pkt, 0x1000017F)), false);
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000200)), true); // V-link switch
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000201)), false);
	assert_int_equal(is_basic_address(set_address(&pkt, 0x1000020F)), false);
	// TODO: Write below
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10000310)), false);
	assert_int_equal(is_basic_address(set_address(&pkt, 0x10001000)), true);
	assert_int_equal(is_basic_address(set_address(&pkt, 0x1000210A)), false);
}

int main()
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_is_basic_address),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
