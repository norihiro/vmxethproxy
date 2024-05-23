#pragma once

#ifdef __cplusplus
#include <vector>
#include <cstdint>
#include <stddef.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct vmxpacket_s;
typedef struct vmxpacket_s vmxpacket_t;

vmxpacket_t *vmxpacket_create();
void vmxpacket_destroy(vmxpacket_t *);

int parse_tcp_stream(vmxpacket_t *p, const uint8_t *buf, int length);
bool vmxpacket_from_string(vmxpacket_t *p, const char *str, uint8_t device_id);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
struct vmxpacket_s
{
	std::vector<uint8_t> raw;
	std::vector<uint8_t> midi;
	bool raw_modified = false;
	bool midi_modified = false;
	int aux = 0;

	std::vector<uint8_t> &modify_midi()
	{
		midi_modified = true;
		return midi;
	}

	std::vector<uint8_t> &modify_raw()
	{
		raw_modified = true;
		return raw;
	}

	const uint8_t *get_raw() const { return &raw[0]; }
	size_t get_raw_size() const { return raw.size(); }

	int add_from_raw(const uint8_t *buf, size_t length);
	bool make_raw();

	uint32_t dt_address_aligned() const
	{
		return midi.size() > 10 ? midi[7] << 24 | midi[8] << 16 | midi[9] << 8 | midi[10] : 0;
	}
	uint32_t dt_address_packed() const
	{
		return midi.size() > 10 ? midi[7] << 21 | midi[8] << 14 | midi[9] << 7 | midi[10] : 0;
	}
	uint32_t dt_size_packed() const
	{
		if (midi.size() > 14 && midi[6] == 0x11) // RQ1
			return midi[11] << 21 | midi[12] << 14 | midi[13] << 7 | midi[14];
		if (midi.size() >= 13 && midi[6] == 0x12) // DT1
			return midi.size() - 11 - 2;
		return 0;
	}
};

inline void add_midi_sum_eox(std::vector<uint8_t> &midi, size_t ix)
{
	uint8_t sum = 0;

	for (size_t i = ix; i < midi.size(); i++)
		sum += midi[i];
	sum &= 0x7F;
	midi.push_back(sum ? 128 - sum : 0);
	midi.push_back(0xF7);
}
#endif // __cplusplus
