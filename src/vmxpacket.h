#pragma once

#ifdef __cplusplus
#include <vector>
#include <cstdint>
#include <stddef.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

vmxpacket_t *vmxpacket_create();
void vmxpacket_destroy(vmxpacket_t *);

int parse_tcp_stream(vmxpacket_t *p, const uint8_t *buf, int length);

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
};
#endif // __cplusplus
