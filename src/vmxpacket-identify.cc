#include <string>
#include <vector>
#include <cstdint>
#include "vmxethproxy.h"
#include "vmxpacket.h"
#include "vmxpacket-identify.h"

static bool compare_data(const std::vector<uint8_t> &midi, const int *exp)
{
	for (size_t i = 0; exp[i] != -2; i++) {
		if (midi.size() <= i)
			return false;
		if (exp[i] == -1)
			continue;
		if (midi[i] != exp[i])
			return false;
		if (exp[i] == 0xF7)
			return midi.size() == i + 1;
	}
	return true;
}

bool vmxpacket_is_midi_idreq(const vmxpacket_t *pkt)
{
	// clang-format off
	const int exp[] = {
		0xF0,
		0x7E, -1, 0x06, 0x01,
		0xF7
	};
	// clang-format on
	return compare_data(pkt->midi, exp);
}

bool vmxpacket_is_midi_idreply(const vmxpacket_t *pkt)
{
	// clang-format off
	const int exp[] = {
		0xF0,
		0x7E, -1, 0x06, 0x02, 0x41, 0x24, 0x02,
		0x00, 0x03, -1, -1, -1, -1,
		0xF7
	};
	// clang-format on
	return compare_data(pkt->midi, exp);
}

bool vmxpacket_is_midi_dt1(const vmxpacket_t *pkt)
{
	// clang-format off
	const int exp[] = {
		0xF0,
		0x41, -1, 0x00, 0x00, 0x24, 0x12, // DT1
		-1, -1, -1, -1, // address
		-1, // checksum
		-1, // EOX
		-2
	};
	// clang-format on
	return compare_data(pkt->midi, exp);
}

bool vmxpacket_is_midi_rq1(const vmxpacket_t *pkt)
{
	// clang-format off
	const int exp[] = {
		0xF0,
		0x41, -1, 0x00, 0x00, 0x24, 0x11, // RQ1
		-1, -1, -1, -1, // address
		-1, -1, -1, -1, // size
		-1, // checksum
		0xF7 // EOX
	};
	// clang-format on
	return compare_data(pkt->midi, exp);
}
