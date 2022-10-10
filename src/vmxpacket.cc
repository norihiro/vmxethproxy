#include <cstdio>
#include "vmxethproxy.h"
#include "vmxpacket.h"

vmxpacket_t *vmxpacket_create()
{
	return new struct vmxpacket_s;
}

void vmxpacket_destroy(vmxpacket_t *p)
{
	delete p;
}

int vmxpacket_s::add_from_raw(const uint8_t *buf, size_t length)
{
	if (length < 8)
		return 0;

	midi.reserve(midi.size() + (length - 8) * 3 / 4);

	if (buf[0] != 0x02 || buf[1] != 0x00)
		return -1;

	size_t exp_length = (buf[2] << 8 | buf[3]) + 4;
	if (length < exp_length)
		return 0;

	if (exp_length <= 8)
		return -1;

	if (exp_length & 3)
		return -1;

	if (buf[4] != 0x00 || buf[5] != 0x00 || buf[6] != 0x00 || buf[7] != 0x00)
		return -1;

	aux = buf[8] >> 4;
	for (size_t i = 8; i < exp_length; i += 4) {
		if ((buf[i] >> 4) != aux) {
			fprintf(stderr, "Error: aux data mismatch, ignoring %#02x %#02x\n", aux, buf[i] >> 4);
		}
		if ((buf[i] & 0x03) != 0 && i + 4 != exp_length) {
			fprintf(stderr,
				"Error: expecting MIDI data end but length mismatch i=%d exp_length=%d, ignoring.\n",
				(int)i, (int)exp_length);
		}
		switch (buf[i] & 0x0F) {
		case 0x04:
		case 0x07:
			midi.push_back(buf[i + 1]);
			midi.push_back(buf[i + 2]);
			midi.push_back(buf[i + 3]);
			break;
		case 0x05:
			midi.push_back(buf[i + 1]);
			break;
		case 0x06:
			midi.push_back(buf[i + 1]);
			midi.push_back(buf[i + 2]);
			break;
		default:
			fprintf(stderr, "Error: unknown header of quartet %#02x at i=%d, ignoring.\n", buf[i], (int)i);
		}
	}

	raw.insert(raw.end(), buf, buf + exp_length);
	raw_modified = midi_modified = false;
	return exp_length;
}

static bool is_dt1(const std::vector<uint8_t> &midi)
{
	if (midi.size() < 6)
		return false;
	if (midi[0] == 0xF0 && midi[1] == 0x41 && midi[6] == 0x12)
		return true;
	return false;
}

static bool has_eox(const std::vector<uint8_t> &midi)
{
	for (uint8_t x : midi) {
		if (x == 0xF7)
			return true;
	}
	return false;
}

int parse_tcp_stream(vmxpacket_t *p, const uint8_t *buf, int length)
{
	if (length < 4)
		return 0;

	if (buf[0] == 0x00 && buf[1] == 0x00 && buf[2] == 0x00 && buf[3] == 0x00)
		return 4;

	if (buf[0] == 0x02 && buf[1] == 0x00) {
		int ret = p->add_from_raw(buf, length);
		if (ret <= 0)
			return ret;

		if (is_dt1(p->midi) && !has_eox(p->midi)) {
			int ret1 = parse_tcp_stream(p, buf + ret, length - ret);
			if (ret1 <= 0)
				return ret1;
			return ret + ret1;
		}

		return ret;
	}

	return -1;
}

bool vmxpacket_s::make_raw()
{
	raw.resize(8);
	size_t exp_length = 4 + (midi.size() + 2) / 3 * 4;
	raw.reserve(4 + exp_length);

	raw[0] = 0x02;
	raw[1] = 0x00;
	raw[2] = exp_length >> 8;
	raw[3] = exp_length & 0xFF;
	raw[4] = 0x00;
	raw[5] = 0x00;
	raw[6] = 0x00;
	raw[7] = 0x00;

	// TODO: when midi size is too large, separate into different packet.
	// Packet from iPad to V-Mixer is limited by 0x40 bytes (42 bytes in MIDI).

	for (size_t i = 0; i < midi.size(); i += 3) {
		size_t rem = midi.size() - i;
		if (rem == 1) {
			raw.push_back(0x05 | (aux << 4));
			raw.push_back(midi[i]);
			raw.push_back(0);
			raw.push_back(0);
		}
		else if (rem == 2) {
			raw.push_back(0x06 | (aux << 4));
			raw.push_back(midi[i]);
			raw.push_back(midi[i + 1]);
			raw.push_back(0);
		}
		else if (rem == 3) {
			raw.push_back(0x07 | (aux << 4));
			raw.push_back(midi[i]);
			raw.push_back(midi[i + 1]);
			raw.push_back(midi[i + 2]);
		}
		else {
			raw.push_back(0x04 | (aux << 4));
			raw.push_back(midi[i]);
			raw.push_back(midi[i + 1]);
			raw.push_back(midi[i + 2]);
		}
	}

	if (raw.size() != exp_length + 4) {
		fprintf(stderr, "Error: exp_length mismatch %d %d\n", (int)raw.size(), (int)exp_length);
		return false;
	}

	raw_modified = midi_modified = false;
	return true;
}
