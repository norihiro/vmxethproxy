#include <cstdio>
#include <cstring>
#include <vector>
#include <cctype>
#include "vmxethproxy.h"
#include "vmxpacket.h"

static char *parse_string(char *&buf)
{
	while (*buf && isspace(*buf))
		buf++;
	if (!*buf)
		return NULL;

	char *ret = buf;

	while (*buf && !isspace(*buf))
		buf++;
	if (*buf) {
		*buf++ = 0;
	}

	return ret;
}

template<typename T> bool parse_hexint(char *&buf, T &result)
{
	char *s = parse_string(buf);
	if (!s)
		return false;

	for (result = 0; *s; s++) {
		char c = *s;
		if ('0' <= c && c <= '9')
			result = result * 16 + c - '0';
		else if ('A' <= c && c <= 'F')
			result = result * 16 + c - 'A' + 10;
		else if ('a' <= c && c <= 'f')
			result = result * 16 + c - 'a' + 10;
		else
			return false;
	}
	return true;
}

static void add_midi_sum_eox(std::vector<uint8_t> &midi, size_t ix)
{
	uint8_t sum = 0;

	for (size_t i = ix; i < midi.size(); i++)
		sum += midi[i];
	sum &= 0x7F;
	midi.push_back(sum ? 128 - sum : 0);
	midi.push_back(0xF7);
}

bool vmxpacket_from_string(vmxpacket_t *p, const char *str, uint8_t device_id)
{
	std::vector<char> buf_v(str, str + strlen(str) + 1);
	char *buf = &buf_v[0];

	std::vector<uint8_t> &midi = p->modify_midi();

	char *name = parse_string(buf);
	if (!name) {
		fprintf(stderr, "Error: Cannot parse command in '%s'\n", str);
		return false;
	}

	if (strcmp(name, "DT1") == 0) {
		uint32_t addr;
		if (!parse_hexint(buf, addr)) {
			fprintf(stderr, "Error: %s: Cannot parse address in '%s'\n", name, str);
			return false;
		}

		midi = std::vector<uint8_t>{
			0xF0,
			0x41,
			device_id,
			0x00,
			0x00,
			0x24,
			0x12, // DT1
			(uint8_t)((addr >> 24) & 0x7F),
			(uint8_t)((addr >> 16) & 0x7F), // address MSP
			(uint8_t)((addr >> 8) & 0x7F),
			(uint8_t)(addr & 0x7F), // address LSB
		};

		uint8_t data;
		while (parse_hexint(buf, data)) {
			midi.push_back(data);
		}

		add_midi_sum_eox(midi, 7);
		return p->make_raw();
	}

	if (strcmp(name, "RQ1") == 0) {
		uint32_t addr, size;
		if (!parse_hexint(buf, addr)) {
			fprintf(stderr, "Error: %s: Cannot parse address in '%s'\n", name, str);
			return false;
		}

		if (!parse_hexint(buf, size)) {
			fprintf(stderr, "Error: %s: Cannot parse size in '%s'\n", name, str);
			return false;
		}

		midi = std::vector<uint8_t>{
			0xF0,
			0x41,
			device_id,
			0x00,
			0x00,
			0x24,
			0x11, // RQ1
			(uint8_t)((addr >> 24) & 0x7F),
			(uint8_t)((addr >> 16) & 0x7F), // address MSP
			(uint8_t)((addr >> 8) & 0x7F),
			(uint8_t)(addr & 0x7F), // address LSB
			(uint8_t)((size >> 24) & 0x7F),
			(uint8_t)((size >> 16) & 0x7F), // size MSP
			(uint8_t)((size >> 8) & 0x7F),
			(uint8_t)(size & 0x7F), // size LSB
		};

		add_midi_sum_eox(midi, 7);
		return p->make_raw();
	}

	return false;
}
