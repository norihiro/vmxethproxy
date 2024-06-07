#include "vmxethproxy.h"
#include "proxycore.h"
#include "proxycore-internal.hh"
#include "vmxpacket.h"
#include "vmxpacket-identify.h"

proxycore_t *proxycore_create()
{
	auto p = new struct proxycore_s;
	return p;
}

void proxycore_destroy(proxycore_t *p)
{
	delete p;
}

void proxycore_add_instance(proxycore_t *p, proxycore_instance_cb callback, void *data, uint32_t flags)
{
	if ((flags & (PROXYCORE_INSTANCE_HOST | PROXYCORE_INSTANCE_PRIMARY)) == 0)
		flags |= PROXYCORE_INSTANCE_SECONDARY;
	instance_info_s info = {
		callback,
		data,
		flags,
	};
	p->instances.push_back(info);

	if (flags & PROXYCORE_INSTANCE_HOST) {
		vmxpacket_t pkt;
		pkt.modify_midi() = std::vector<uint8_t>{
			0xF0,                   // system exclusive message
			0x7E, 0x7F, 0x06, 0x01, // ID request
			0xF7                    // EOX
		};
		if (pkt.make_raw()) {
			callback(&pkt, NULL, data);
		}
	}
}

void proxycore_remove_instance(proxycore_t *p, proxycore_instance_cb callback, void *data)
{
	if (!p)
		return;

	for (auto it = p->instances.begin(); it != p->instances.end(); it++) {
		if (it->callback == callback && it->data == data) {
			p->instances.erase(it);
			return;
		}
	}
}

static bool reply_to_sender(proxycore_t *p, const vmxpacket_t *packet, const void *sender)
{
	for (auto &inst : p->instances) {
		if (inst.data == sender) {
			inst.callback(packet, NULL, inst.data);
			return true;
		}
	}
	return false;
}

static bool is_basic_address(const vmxpacket_t *packet)
{
	if (vmxpacket_is_midi_dt1(packet) || vmxpacket_is_midi_rq1(packet)) {
		int addr = packet->dt_address_aligned();
		if (addr < 0x10000000)
			return true;
		else if (0x10000000 <= addr && addr < 0x10000004)
			return true;
		else if (0x10000008 == addr)
			return true;
		else if (0x10000010 <= addr && addr < 0x10000016)
			return true;
		else if (0x10000020 <= addr && addr < 0x10000021)
			return true;
		else if (0x10000022 <= addr && addr < 0x10000026)
			return true;
		else if (0x10000027 <= addr && addr < 0x10000029)
			return true;
		else if (0x10000100 <= addr && addr < 0x10000104)
			return true;
		else if (0x10000110 <= addr && addr < 0x10000113)
			return true;
		else if (0x10000114 == addr)
			return true;
		else if (0x10000118 <= addr && addr < 0x1000011B)
			return true;
		else if (0x10000120 <= addr && addr < 0x10000123)
			return true;
		else if (0x10000124 == addr)
			return true;
		else if (0x10000128 <= addr && addr < 0x1000012B)
			return true;
		else if (0x10000200 == addr)
			return true;
		else if (0x10000210 <= addr && addr < 0x10000310)
			return true;
		else if (0x10001000 <= addr && addr < 0x1000210A)
			return true;
	}

	return false;
}

static bool return_by_proxy(proxycore_t *p, const vmxpacket_t *packet, const void *sender, uint32_t sender_flags)
{
	if (sender_flags & (PROXYCORE_INSTANCE_HOST | PROXYCORE_INSTANCE_PRIMARY))
		return false;

	/* Devices other than host nor primary are not supposed to be there.
	 * Try to hide from the host or primary. */

	if (vmxpacket_is_midi_idreq(packet) && p->host_id >= 0) {
		vmxpacket_t pkt_reply;
		// clang-format off
		pkt_reply.modify_midi() = std::vector<uint8_t>{
			0xF0, 0x7E, (uint8_t)p->host_id, // Device ID
			0x06, 0x02, // ID reply
			0x41, 0x24, 0x02, 0x00, 0x03, // manufacturer and device ID
			p->host_id_revision[0], p->host_id_revision[1], p->host_id_revision[2],
			p->host_id_revision[3], // software revision
			0xF7 };
		// clang-format on
		pkt_reply.make_raw();

		if (reply_to_sender(p, &pkt_reply, sender))
			return true;
	}

	if (vmxpacket_is_midi_rq1(packet) && is_basic_address(packet)) {
		int addr = packet->dt_address_packed();
		int size = packet->dt_size_packed();
		for (int i = 0; i < size; i++) {
			if (p->dt1_cache.count(addr + i) == 0)
				return false;
		}

		vmxpacket_t pkt_reply;
		auto &midi = pkt_reply.modify_midi();
		midi = std::vector<uint8_t>{
			0xF0,
			0x41,
			(uint8_t)(p->host_id & 0x7F),
			0x00,
			0x00,
			0x24,
			0x12, // DT1
			(uint8_t)((addr >> 21) & 0x7F),
			(uint8_t)((addr >> 14) & 0x7F), // address MSP
			(uint8_t)((addr >> 7) & 0x7F),
			(uint8_t)(addr & 0x7F), // address LSB
		};

		for (int i = 0; i < size; i++)
			midi.push_back(p->dt1_cache[addr + i]);

		add_midi_sum_eox(midi, 7);
		if (pkt_reply.make_raw()) {
			reply_to_sender(p, &pkt_reply, sender);
			return true;
		}
	}

	return false;
}

static uint32_t determine_receiver_types(proxycore_t *p, const vmxpacket_t *packet, uint32_t sender_flags)
{
	(void)p;

	if (is_basic_address(packet))
		return PROXYCORE_INSTANCE_HOST | PROXYCORE_INSTANCE_PRIMARY | PROXYCORE_INSTANCE_SECONDARY;
	if (sender_flags & PROXYCORE_INSTANCE_HOST)
		return PROXYCORE_INSTANCE_PRIMARY;
	if (sender_flags & PROXYCORE_INSTANCE_PRIMARY)
		return PROXYCORE_INSTANCE_HOST;

	return 0;
}

static bool preprocess_packet(proxycore_t *p, vmxpacket_t *packet, uint32_t sender_flags)
{
	if ((sender_flags & PROXYCORE_INSTANCE_HOST) && vmxpacket_is_midi_idreply(packet)) {
		p->host_id = packet->midi[2];
		p->host_id_revision[0] = packet->midi[10];
		p->host_id_revision[1] = packet->midi[11];
		p->host_id_revision[2] = packet->midi[12];
		p->host_id_revision[3] = packet->midi[13];
	}

	if (vmxpacket_is_midi_dt1(packet)) {
		int addr = packet->dt_address_packed();
		int size = packet->dt_size_packed();
		for (int i = 0; i < size; i++)
			p->dt1_cache[addr + i] = packet->midi[11 + i];
	}

	return true;
}

void proxycore_process_packet(proxycore_t *p, vmxpacket_t *packet, const void *sender, uint32_t sender_flags)
{
	if (return_by_proxy(p, packet, sender, sender_flags))
		return;

	uint32_t receiver_types = determine_receiver_types(p, packet, sender_flags) | PROXYCORE_INSTANCE_MONITOR;

	if (!preprocess_packet(p, packet, sender_flags))
		return;

	// TODO: modify data if necessary

	for (auto &inst : p->instances) {
		if ((inst.flags & receiver_types) && inst.data != sender)
			inst.callback(packet, sender, inst.data);
	}
}

int proxycore_get_host_id(const proxycore_t *p)
{
	return p->host_id;
}
